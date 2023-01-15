#include "logging.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include <operations.h>
#include <producer-consumer.h>
#include <state.h>
#include <utils.h>

// Array das structs das caixas
box_t boxes[MAX_BOXES];

// Variaveis de condicao e mutexes, 1 por caixa
pthread_cond_t box_conditions[MAX_BOXES];
pthread_mutex_t box_mutexes[MAX_BOXES];

size_t n_boxes;
size_t max_sessions;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// Threads e queue da fila producer consumer
pthread_t *tid;
pthread_t main_thread;
pc_queue_t *queue;

char *register_pipe_name;


// Liberta a memória antes de fechar o mbroker
void mbroker_close() {
  
    pcq_destroy(queue);

    // fecha os pipes de todos os subscribers
    for (int i = 0; i < n_boxes; i++) {
        for (int j = 0; j < boxes[i].n_subscribers; i++) {
            int tx = open_pipe(boxes[i].subscribers[j], 'w');
            close(tx);
        }
    }

    // Liberta a memória das threads
    for (int i = 0; i < max_sessions; i++)
        free(&tid[i]);
 
    free(queue);

    // FIXME: Não é suposto??

    unlink(register_pipe_name);
}

// Sigint handler
static void sig_handler(int sig) {

    if (sig != SIGINT)
        return;

    // Catched SIGINT successfully
    mbroker_close();
    exit(EXIT_SUCCESS);
}


void delete_box(int i) {
    pthread_mutex_lock(&mutex);
    char string_aux[PIPE_NAME_SIZE];
    uint64_t int_aux;
    uint8_t last_aux;

    // Se tiver um publisher associado, fecha o pipe, enviando um SIGPIPE caso tente escrever
    if (boxes[i].n_publishers == 1) {

        char publisher_pipe[PIPE_NAME_SIZE];
        strcpy(publisher_pipe, boxes[i].publisher);

        int tx = open_pipe(publisher_pipe, 'r');
        close(tx);
    }
    // Troca a box com a última do array boxes 
    strcpy(string_aux, boxes[i].box_name);
    strcpy(boxes[i].box_name, boxes[n_boxes - 1].box_name);
    strcpy(boxes[n_boxes - 1].box_name, string_aux);

    strcpy(string_aux, boxes[i].publisher);
    strcpy(boxes[i].publisher, boxes[n_boxes - 1].publisher);
    strcpy(boxes[n_boxes - 1].publisher, string_aux);

    int_aux = boxes[i].box_size;
    boxes[i].box_size = boxes[n_boxes - 1].box_size;
    boxes[n_boxes - 1].box_size = int_aux;

    int_aux = boxes[i].n_publishers;
    boxes[i].n_publishers = boxes[n_boxes - 1].n_publishers;
    boxes[n_boxes - 1].n_publishers = int_aux;

    int_aux = boxes[i].n_subscribers;
    boxes[i].n_subscribers = boxes[n_boxes - 1].n_subscribers;
    boxes[n_boxes - 1].n_subscribers = int_aux;

    last_aux = boxes[i].last;
    boxes[i].last = boxes[n_boxes - 1].last;
    boxes[n_boxes - 1].last = last_aux;

    pthread_cond_destroy(&box_conditions[n_boxes]);
    pthread_mutex_destroy(&box_mutexes[n_boxes]);

    n_boxes--;
    pthread_mutex_unlock(&mutex);
}

void add_box(box_t box) {
    pthread_mutex_lock(&mutex);
    boxes[n_boxes] = box;
    // inicializa as var. de condição e o mutex da caixa 
    pthread_cond_init(&box_conditions[n_boxes], NULL);
    pthread_mutex_init(&box_mutexes[n_boxes], NULL);
    n_boxes++;
    pthread_mutex_unlock(&mutex);
}

// Retorna o indice da caixa no array,
// ou -1 caso não exista
int find_box(char *box_name) {
    char *name;
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < n_boxes; i++) {
        name = boxes[i].box_name;

        if (!strcmp(name, box_name)) {
            pthread_mutex_unlock(&mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1;
}

void add_subscriber_to_box(int box_index, char *subscriber_pipe_name) {
    pthread_mutex_lock(&mutex);
    int i = boxes[box_index].subscriber_index;
    strncpy(boxes[box_index].subscribers[i], subscriber_pipe_name,
            PIPE_NAME_SIZE);

    boxes[box_index].subscriber_index++;
    pthread_mutex_unlock(&mutex);
}

// Função do publisher
// Ver - verifica se passou no registo (1 se registou, 0 caso contrário)
void recieve_messages_from_publisher(register_request_t publisher,
                                     int box_index, int ver) {

    char box_name[BOX_NAME_SIZE + 1];
    // Acrescenta '/' ao pipename
    box_name[0] = '/';
    strcpy(box_name + 1, publisher.box_name);

    int box_handle = tfs_open(box_name, TFS_O_APPEND);

    uint8_t code;
    
    char message[MESSAGE_SIZE];
    char buffer[MESSAGE_SIZE];

    // Abre o pipe do cliente para leitura
    int pub_pipe = open_pipe(publisher.client_name_pipe_path, 'r');

    // Não passou no registo - fecha o pipe e dá dequeue
    if (ver == 0) {
        close(pub_pipe);
        pcq_dequeue(queue);
        return;
    }

    ssize_t bytes_read = read_pipe(pub_pipe, &code, sizeof(uint8_t));

    while (bytes_read > 0 && code == 9) {
        // Caso o publisher não esteja ligado a nenhuma box
        if ((box_index = find_box(publisher.box_name)) == -1) {
            close(pub_pipe);
            break;
        }
        // le a mensagem e acrescenta \0
        read_pipe(pub_pipe, &buffer, MESSAGE_SIZE);
        strcpy(message, buffer);
        message[strlen(buffer)] = '\0';

       
        pthread_mutex_lock(&box_mutexes[box_index]);

        // Escreve a mensagem na box
        tfs_write(box_handle, message, strlen(message));
        tfs_write(box_handle, "\0", sizeof(char));
        boxes[box_index].box_size += strlen(message);

        pthread_cond_broadcast(&box_conditions[box_index]);
        pthread_mutex_unlock(&box_mutexes[box_index]);

        bytes_read = read_pipe(pub_pipe, &code, sizeof(uint8_t));
    }
    if (box_index != -1) {
        boxes[box_index].n_publishers = 0;
        close(pub_pipe);
    }

    // Fim do publisher
    pcq_dequeue(queue);
    tfs_close(box_handle);
}

void *publisher_function(void *args) {
    thread_args const *arguments = (thread_args const *)args;

    register_request_t request;

    strcpy(request.client_name_pipe_path, arguments->client_name_pipe_path);
    strcpy(request.box_name, arguments->box_name);

    // Acrescenta '/' ao pipename
    char box_name[BOX_NAME_SIZE + 1];
    box_name[0] = '/';
    strcpy(box_name + 1, request.box_name);

    int box_handle = tfs_open(box_name, TFS_O_APPEND);

    int box_index = find_box(request.box_name);

    // Struct ou ficheiro da box não existem
    if (box_index == -1 || box_handle == -1) {
        recieve_messages_from_publisher(request, box_index, 0);
        return NULL;
    }
    // Já tem um publisher ligado 
    if (boxes[box_index].n_publishers == 1) {
        recieve_messages_from_publisher(request, box_index, 0);
        return NULL;
    }

    // Success!!
    boxes[box_index].n_publishers = 1;
    strcpy(boxes[box_index].publisher, arguments->client_name_pipe_path);

    // Chama a função do publisher
    recieve_messages_from_publisher(request, box_index, 1);
    return NULL;
}

// Função do publisher
// Ver - verifica se passou no registo (1 se registou, 0 caso contrário)
void read_messages(register_request_t subscriber_request, int num) {

    char box_name[BOX_NAME_SIZE + 1];
    box_name[0] = '/';
    strcpy(box_name + 1, subscriber_request.box_name);
    int box_handle = tfs_open(box_name, 0);
    int box_index = find_box(subscriber_request.box_name);

    int subscriber_pipe =
        open_pipe(subscriber_request.client_name_pipe_path, 'w');
    
    // Não passou no registo
    if (num == 0)
        close(subscriber_pipe);

    // Box não existe
    if (box_handle == -1 || box_index == -1) {
        pcq_dequeue(queue);
        return;
    }

    // Success!!!

    uint8_t code = 10;
    char char_buffer;
    char buffer[MESSAGE_SIZE];
    int i = 0, j = 0;
    // ssize_t bytes_read = tfs_read(box_handle, &char_buffer, sizeof(char));

    while (true) {
        // Box removida
        if ((box_index = find_box(subscriber_request.box_name)) == -1) {
            close(subscriber_pipe);
            break;
        }

        if (pthread_mutex_lock(&box_mutexes[box_index]) != 0) {
            j++;
            exit(EXIT_FAILURE);
        }
        ssize_t bytes_read;

        // Espera ate ter algo para ler
        while ((bytes_read =
                    tfs_read(box_handle, &char_buffer, sizeof(char))) == 0) {
            pthread_cond_wait(&box_conditions[box_index],
                              &box_mutexes[box_index]);

            if ((box_index = find_box(subscriber_request.box_name)) == -1) {
                close(subscriber_pipe);
                break;
            }
        }
        
        // Fim da mensagem
        if (char_buffer == '\0') {
            buffer[i] = '\0';
            fill_string(MESSAGE_SIZE, buffer);
            
            // Envia ao subscriber a mensagem
            if (write(subscriber_pipe, &code, sizeof(uint8_t)) < 1) {
                exit(EXIT_FAILURE);
            }
            if (write(subscriber_pipe, &buffer, MESSAGE_SIZE) < 1) {
                exit(EXIT_FAILURE);
            }

            // reset buffer e indíce
            memset(buffer, 0, MESSAGE_SIZE);
            i = 0;

        // Ainda nao terminou
        } else {
            buffer[i] = char_buffer;
            i++;
        }

        if (j == 0)
            pthread_mutex_unlock(&box_mutexes[box_index]);
        j = 0;
    }
    if(boxes[box_index].n_subscribers > 0 && box_index >= 0) 
        boxes[box_index].n_subscribers = boxes[box_index].n_subscribers - 1;
    
    // Acabou o subscriber
    tfs_close(box_handle);
    close(subscriber_pipe);

    pcq_dequeue(queue);
}

void *register_subscriber(void *args) {
    thread_args const *arguments = (thread_args const *)args;

    register_request_t subscriber_request;
    strcpy(subscriber_request.client_name_pipe_path,
           arguments->client_name_pipe_path);
    strcpy(subscriber_request.box_name, arguments->box_name);

    int box_handle = find_box(subscriber_request.box_name);

    // Box não existe
    if (box_handle < 0) {
        read_messages(subscriber_request, 0);
        return NULL;
    }

    int box_index = find_box(subscriber_request.box_name);

    // Box não existe
    if (box_index == -1) {
        read_messages(subscriber_request, 0);
        return NULL;
    }

    // Acrescenta o novo sub à box
    boxes[box_index].n_subscribers++;
    add_subscriber_to_box(box_index, subscriber_request.client_name_pipe_path);

    // Subscriber created sucessfully, will wait for messages to be written in
    // message box
    read_messages(subscriber_request, 1);

    return NULL;
}

void reply_to_box_creation(char *pipe_name, int n) {

    box_reply_t reply;
    uint8_t code = 4;

    reply.return_code = 0;

    // error creating box
    if (n == 0) {
        reply.return_code = -1;
        fill_string(MESSAGE_SIZE, reply.error_message);
        strcpy(reply.error_message, "Couldn't create box");
    }

    int manager_pipe = open_pipe(pipe_name, 'w');
    fill_string(MESSAGE_SIZE, reply.error_message);

    // Resposta à criação de uma caixa - escreve para o manager
    if (write(manager_pipe, &code, sizeof(uint8_t)) < 1) {
        exit(EXIT_FAILURE);
    }
    if (write(manager_pipe, &reply, sizeof(box_reply_t)) < 1) {
        exit(EXIT_FAILURE);
    }

    // Sucess!!!
    close(manager_pipe);

    pcq_dequeue(queue);
}

void *create_box(void *args) {
    thread_args const *arguments = (thread_args const *)args;

    box_t box;
    register_request_t box_request;
    strcpy(box_request.client_name_pipe_path, arguments->client_name_pipe_path);
    strcpy(box_request.box_name, arguments->box_name);

    int box_index = find_box(box_request.box_name);

    // the box already exists
    if (box_index != -1) {
        reply_to_box_creation(box_request.client_name_pipe_path, 0);
        return NULL;
    }

    char box_name[BOX_NAME_SIZE + 1];
    // Acrescenta / ao pipename
    box_name[0] = '/';
    strcpy(box_name + 1, box_request.box_name);

    // Cria o ficheiro da caixa
    int box_handle = tfs_open(box_name, TFS_O_CREAT);

    // Erro na criação
    if (box_handle == -1) { 
        reply_to_box_creation(box_request.client_name_pipe_path, 0);
        return NULL;
    }

    // Criação da struct
    box.box_size = 0;
    box.n_publishers = 0;
    box.n_subscribers = 0;
    strcpy(box.box_name, box_request.box_name);


    tfs_close(box_handle);

    // adicionar box ao array de boxes
    add_box(box);
    reply_to_box_creation(box_request.client_name_pipe_path, 1);
    return NULL;
}

void reply_to_box_removal(char *pipe_name, int n) {

    box_reply_t reply;

    uint8_t code = 6;
    reply.return_code = 0;

    fill_string(MESSAGE_SIZE, reply.error_message);

    // pedido de remoção inválido
    if (n == 0) {
        reply.return_code = -1;
        strcpy(reply.error_message, "Couldn't remove box");
    }

    int manager_pipe = open_pipe(pipe_name, 'w');

    // Resposta ao pedido de remoção enviada ao manager
    if (write(manager_pipe, &code, sizeof(uint8_t)) < 1) {
        printf("Writing error\n");
        exit(EXIT_FAILURE);
    }
    if (write(manager_pipe, &reply, sizeof(box_reply_t)) < 1) {
        printf("Writing error\n");
        exit(EXIT_FAILURE);
    }

    // Fim do manager
    pcq_dequeue(queue);
}

void *remove_box(void *args) {
    thread_args const *arguments = (thread_args const *)args;

    register_request_t box_request;
    char box_name[BOX_NAME_SIZE + 1];
    box_name[0] = '/';

    strcpy(box_request.client_name_pipe_path, arguments->client_name_pipe_path);
    strcpy(box_request.box_name, arguments->box_name);

    strcpy(box_name + 1, box_request.box_name);

    // unlink file associated to box
    int box_handle = tfs_unlink(box_name);
    // Error
    if (box_handle == -1) {
        reply_to_box_removal(box_request.client_name_pipe_path, 0);
        return NULL;
    }

    // verifica se está no array global
    int box_index = find_box(box_request.box_name);

    // Box não está no array global
    if (box_index == -1) {
        reply_to_box_removal(box_request.client_name_pipe_path, 0);
        return NULL;
    }

    // free box from array
    pthread_cond_broadcast(&box_conditions[box_index]);
    delete_box(box_index);
    reply_to_box_removal(box_request.client_name_pipe_path, 1);

    return NULL;
}

void reply_to_list_boxes(char *manager_pipe) {

    uint8_t code = 8;
    box_t reply_box;

    int man_pipe = open_pipe(manager_pipe, 'w');

    int i = 0;

    if (write(man_pipe, &code, sizeof(uint8_t)) < 1) {
        exit(EXIT_FAILURE);
    }
    // Não existem boxes: retorna uma box com box name = '\0' e last = 1
    if (n_boxes == 0) {
        reply_box.last = 1;
        
        strcpy(reply_box.publisher, "\0");
        strcpy(reply_box.box_name, "\0");

        reply_box.box_size = 0;
        reply_box.n_publishers = 0;
        reply_box.n_subscribers = 0;

        // Envio
        if (write(man_pipe, &reply_box, sizeof(box_t)) < 1)
            exit(EXIT_FAILURE);

    // Existem boxes - lista-as
    } else {
        pthread_mutex_lock(&mutex);
        boxes[n_boxes - 1].last = 1;
        pthread_mutex_unlock(&mutex);

        for (i = 0; i < n_boxes; i++) {
            reply_box = boxes[i];

            if (write(man_pipe, &reply_box, sizeof(box_t)) < 1)
                exit(EXIT_FAILURE);
        }
        // Reseta o last para no segundo list não bloquear neste elemento
        boxes[n_boxes - 1].last = 0;
    }
    // Fim da listagem
    pcq_dequeue(queue);
}

void *list_boxes(void *args) {
    char const *arguments = (char const *)args;

    char manager_pipe[PIPE_NAME_SIZE];

    strcpy(manager_pipe, arguments);

    reply_to_list_boxes(manager_pipe);
    return NULL;
}

void *main_thread_function(void *arg) {
    int *register_pipe = (int *)arg;

    uint8_t code;

    ssize_t bytes_read;
    register_request_t box_request;

    thread_args args;
    size_t index;

    // Register pipe funciona
    while ((bytes_read = read_pipe(*register_pipe, &code, sizeof(uint8_t))) !=
           -1) {

        switch (code) {
        case 1:
            // register publisher
            index = queue->pcq_tail;

            if (read_pipe(*register_pipe, &box_request,
                          sizeof(register_request_t)) < 1)
                exit(EXIT_FAILURE);

            strcpy(args.client_name_pipe_path,
                   box_request.client_name_pipe_path);
            strcpy(args.box_name, box_request.box_name);

            // Há espaço para a thread
            if (pcq_enqueue(queue, &tid[index]) == 0) {
                // Falha ao criar a thread
                if (pthread_create(&tid[index], NULL, publisher_function,
                                   (void *)&args) != 0) {
                    exit(EXIT_FAILURE);
                }
            }
            break;
        case 2:
            // register subscriber
            index = queue->pcq_tail;
            if (read_pipe(*register_pipe, &box_request,
                          sizeof(register_request_t)) < 1)
                exit(EXIT_FAILURE);

            strcpy(args.client_name_pipe_path,
                   box_request.client_name_pipe_path);
            strcpy(args.box_name, box_request.box_name);

            // Há espaço para a thread
            if (pcq_enqueue(queue, &tid[index]) == 0) {
                // Falha ao criar a thread
                if (pthread_create(&tid[index], NULL, register_subscriber,
                                   (void *)&args) != 0) {
                    exit(EXIT_FAILURE);
                }
            }
            break;
        case 3:
            // create box
            index = queue->pcq_tail;
            if (read_pipe(*register_pipe, &box_request,
                          sizeof(register_request_t)) < 1)
                exit(EXIT_FAILURE);

            strcpy(args.client_name_pipe_path,
                   box_request.client_name_pipe_path);
            strcpy(args.box_name, box_request.box_name);

            // Há espaço para a thread
            if (pcq_enqueue(queue, &tid[index]) == 0) {
                // Falha ao criar a thread
                if (pthread_create(&tid[index], NULL, create_box,
                                   (void *)&args) != 0) {
                    exit(EXIT_FAILURE);
                }
            }
            break;
        case 5:
            // box removal
            index = queue->pcq_tail;
            if (read_pipe(*register_pipe, &box_request,
                          sizeof(register_request_t)) < 1)
                exit(EXIT_FAILURE);

            strcpy(args.client_name_pipe_path,
                   box_request.client_name_pipe_path);
            strcpy(args.box_name, box_request.box_name);

            // Há espaço para a thread
            if (pcq_enqueue(queue, &tid[index]) == 0) {
                // Falha ao criar a thread
                if (pthread_create(&tid[index], NULL, remove_box,
                                   (void *)&args) != 0) {
                    exit(EXIT_FAILURE);
                }
            }
            break;
        case 7:
            // box listing
            index = queue->pcq_tail;
            char pipe_name[PIPE_NAME_SIZE];
            if (read_pipe(*register_pipe, &pipe_name, PIPE_NAME_SIZE) < 1)
                exit(EXIT_FAILURE);

            // Há espaço para a thread
            if (pcq_enqueue(queue, &tid[index]) == 0) {
                // Falha ao criar a thread
                if (pthread_create(&tid[index], NULL, list_boxes,
                                   (void *)&pipe_name) != 0) {
                    exit(EXIT_FAILURE);
                }
            }
            break;
        default:
            break;
        }
        // Após receber o código, reseta-o, caso contrário a main thread bloqueia
        code = 0;
    }

    // Fim da main thread - register pipe fechou
    return NULL;
}

int main(int argc, char **argv) {
    // Can't catch SIGINT
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        exit(EXIT_FAILURE);

    if(argc != 3){
        exit(EXIT_FAILURE);
    }

    // Inicializa o file system
    tfs_init(NULL);
    n_boxes = 0;

    register_pipe_name = argv[1];

    // Max-sessions
    long max_s = strtol(argv[2], NULL, 0);

    max_sessions = (size_t)max_s;

    // Aloca espaço para threads e fila producer consumer
    tid = malloc(sizeof(pthread_t) * max_sessions);
    queue = (pc_queue_t *)malloc(sizeof(pc_queue_t));
    
    // Cria fila
    pcq_create(queue, max_sessions);

    // Cria register_pipe
    if (!create_pipe(register_pipe_name))
        return -1;

    int register_pipe = open_pipe(register_pipe_name, 'r');

    // Cria main thread
    if (pthread_create(&main_thread, NULL, main_thread_function,
                       (void *)&register_pipe) != 0) {
        fprintf(stderr, "[ERR]: Fail to create main thread: %s\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    pthread_join(main_thread, NULL);

    // main thread acabou - fecha o mbroker
    mbroker_close();

    return 0;
}
