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

box_t boxes[MAX_BOXES];
size_t n_boxes;
int n_active_threads = 0;
pthread_t *tid;
size_t max_sessions;
pc_queue_t *queue;;

// Liberta a memória antes de fechar o mbroker
void mbroker_close(){
    pcq_destroy(queue);

    for(int i = 0; i < max_sessions; i++)
        free(&tid[i]);

    free(queue);

    printf("That's all folks!\n");
}

static void sig_handler(int sig) {


    if (sig != SIGINT) 
        return; 

    // Catched SIGINT successfully
    mbroker_close();
    exit(EXIT_SUCCESS);
}

void delete_box(int i) {
    char string_aux[PIPE_NAME_SIZE];
    uint64_t int_aux;
    uint8_t last_aux;

    if(boxes[i].n_publishers == 1){

        char publisher_pipe[PIPE_NAME_SIZE];
        strcpy(publisher_pipe, boxes[i].publisher);
        printf("\n\n%s\n\n", publisher_pipe);
        int tx = open_pipe(publisher_pipe, 'r');
        close(tx);
        
    }
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

    n_boxes--;
}

void add_box(box_t box) {
    boxes[n_boxes] = box;
    n_boxes++;
}

int find_box(char *box_name) {
    char *name;
    for (int i = 0; i < n_boxes; i++) {
        name = boxes[i].box_name;

        if (!strcmp(name, box_name)) {
            return i;
        }
    }
    return -1;
}

void add_subscriber_to_box(int box_index, char *subscriber_pipe_name) {
    int i = boxes[box_index].subscriber_index;
    strncpy(boxes[box_index].subscribers[i], subscriber_pipe_name,
            PIPE_NAME_SIZE);
    boxes[box_index].subscriber_index++;
}

void recieve_messages_from_publisher(register_request_t publisher,
                                     int box_index, int ver) {

    char box_name[BOX_NAME_SIZE + 1];
    box_name[0] = '/';
    strcpy(box_name + 1, publisher.box_name);

    int box_handle = tfs_open(box_name, TFS_O_APPEND);

    uint8_t code;
    uint8_t subscriber_code = 10;
    char message[MESSAGE_SIZE];
    char buffer[MESSAGE_SIZE];

    int pub_pipe = open_pipe(publisher.client_name_pipe_path, 'r');

    if (ver == 0) {
        close(pub_pipe);
        pcq_dequeue(queue);
        return;
    }

    ssize_t bytes_read = read_pipe(pub_pipe, &code, sizeof(uint8_t));
    while (bytes_read > 0 && code == 9) {
        read_pipe(pub_pipe, &buffer, MESSAGE_SIZE);
        strcpy(message, buffer);
        message[strlen(buffer)] = '\0';
        tfs_write(box_handle, message, strlen(message));
        tfs_write(box_handle, "\0", sizeof(char));
        boxes[box_index].box_size += strlen(message);
        printf("%s\n", message);

        for (int i = 0; i < boxes[box_index].subscriber_index; i++) {
            int subscriber_pipe =
                open_pipe(boxes[box_index].subscribers[i], 'w');
            if (write(subscriber_pipe, &subscriber_code, sizeof(uint8_t)) < 1) {
                exit(EXIT_FAILURE);
            }
            if (write(subscriber_pipe, &buffer, MESSAGE_SIZE) < 1) {
                exit(EXIT_FAILURE);
            }
        }
        bytes_read = read_pipe(pub_pipe, &code, sizeof(uint8_t));
    }
    boxes[box_index].n_publishers = 0;
    close(pub_pipe);

    pcq_dequeue(queue);
    tfs_close(box_handle);
}

void *publisher_function(void *args) {
    thread_args const *arguments = (thread_args const *)args;

    register_request_t request;

    strcpy(request.client_name_pipe_path, arguments->client_name_pipe_path);
    strcpy(request.box_name, arguments->box_name);

    char box_name[BOX_NAME_SIZE + 1];
    box_name[0] = '/';
    strcpy(box_name + 1, request.box_name);

    int box_handle = tfs_open(box_name, TFS_O_APPEND);

    int box_index = find_box(request.box_name);

    if (box_index == -1 || box_handle == -1) {
        recieve_messages_from_publisher(request, box_index, 0);
        return NULL;
    }
    // Success!!
    if (boxes[box_index].n_publishers == 1) {
        recieve_messages_from_publisher(request, box_index, 0);
        return NULL;
    }

    boxes[box_index].n_publishers = 1;
    strcpy(boxes[box_index].publisher, arguments->client_name_pipe_path);

    recieve_messages_from_publisher(request, box_index, 1);
    return NULL;
}

void read_messages(register_request_t subscriber_request, int num) {

    char box_name[BOX_NAME_SIZE + 1];
    box_name[0] = '/';
    strcpy(box_name + 1, subscriber_request.box_name);
    int box_handle = tfs_open(box_name, 0);

    int subscriber_pipe =
        open_pipe(subscriber_request.client_name_pipe_path, 'w');

    if (num == 0)
        close(subscriber_pipe);

    if (box_handle == -1 || subscriber_pipe == -1) {
        pcq_dequeue(queue);
        return;
    }

    uint8_t code = 10;
    char char_buffer;
    char buffer[MESSAGE_SIZE];
    int i = 0;
    ssize_t bytes_read = tfs_read(box_handle, &char_buffer, sizeof(char));

    while (bytes_read > 0) {
        if (char_buffer == '\0') {
            buffer[i] = '\0';

            fill_string(MESSAGE_SIZE, buffer);
            if (write(subscriber_pipe, &code, sizeof(uint8_t)) < 1) {
                exit(EXIT_FAILURE);
            }
            if (write(subscriber_pipe, &buffer, MESSAGE_SIZE) < 1) {
                exit(EXIT_FAILURE);
            }
            // reset buffer e indíce
            memset(buffer, 0, MESSAGE_SIZE);
            i = 0;
        } else {
            buffer[i] = char_buffer;
            i++;
        }
        bytes_read = tfs_read(box_handle, &char_buffer, sizeof(char));
    }

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

    if (box_index == -1) {
        read_messages(subscriber_request, 0);
        return NULL;
    }

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

    if (write(manager_pipe, &code, sizeof(uint8_t)) < 1) {
        exit(EXIT_FAILURE);
    }
    if (write(manager_pipe, &reply, sizeof(box_reply_t)) < 1) {
        exit(EXIT_FAILURE);
    }
    close(manager_pipe);

    pcq_dequeue(queue);
}

void *create_box(void *args) {
    thread_args const *arguments = (thread_args const *)args;

    box_t box;
    register_request_t box_request;
    strcpy(box_request.client_name_pipe_path, arguments->client_name_pipe_path);
    strcpy(box_request.box_name, arguments->box_name);
    printf("box_name:%s, pipe_name:%s\n", box_request.box_name,
           box_request.client_name_pipe_path);

    int box_index = find_box(box_request.box_name);

    // the box already exists
    if (box_index != -1) {
        reply_to_box_creation(box_request.client_name_pipe_path, 0);
        return NULL;
    }

    char box_name[BOX_NAME_SIZE + 1];

    box_name[0] = '/';
    strcpy(box_name + 1, box_request.box_name);

    int box_handle = tfs_open(box_name, TFS_O_CREAT);

    if (box_handle == -1) {
        reply_to_box_creation(box_request.client_name_pipe_path, 0);
        return NULL;
    }

    box.box_size = 0;
    box.n_publishers = 0;
    box.n_subscribers = 0;
    strcpy(box.box_name, box_request.box_name);

    // if (pthread_cond_init(&box.condition, NULL) != 0)
    //     exit(EXIT_FAILURE);

    tfs_close(box_handle);

    // adicionar box a um array de boxes global
    add_box(box);
    reply_to_box_creation(box_request.client_name_pipe_path, 1);
    return NULL;
}

void reply_to_box_removal(char *pipe_name, int n) {

    box_reply_t reply;

    uint8_t code = 6;
    reply.return_code = 0;

    fill_string(MESSAGE_SIZE, reply.error_message);
    // error creating box
    if (n == 0) {
        reply.return_code = -1;
        strcpy(reply.error_message, "Couldn't remove box");
    }

    int manager_pipe = open_pipe(pipe_name, 'w');

    if (write(manager_pipe, &code, sizeof(uint8_t)) < 1) {
        printf("Writing error\n");
        exit(EXIT_FAILURE);
    }
    if (write(manager_pipe, &reply, sizeof(box_reply_t)) < 1) {
        printf("Writing error\n");
        exit(EXIT_FAILURE);
    }
    pcq_dequeue(queue);
}

void *remove_box(void *args) {
    thread_args const *arguments = (thread_args const *)args;

    register_request_t box_request;
    char box_name[BOX_NAME_SIZE + 1];
    box_name[0] = '/';
    strcpy(box_request.client_name_pipe_path, arguments->client_name_pipe_path);
    strcpy(box_request.box_name, arguments->box_name);

    printf("box_name:%s, pipe_name:%s\n", box_request.box_name,
           box_request.client_name_pipe_path);

    strcpy(box_name + 1, box_request.box_name);

    // unlink file associated to box
    int box_handle = tfs_unlink(box_name);

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
    if (n_boxes == 0) {
        reply_box.last = 1;
        // fill_string(PIPE_NAME_SIZE, reply_box.publisher);
        // fill_string(PIPE_NAME_SIZE, reply_box.box_name);
        strcpy(reply_box.publisher, "\0");
        strcpy(reply_box.box_name, "\0");

        reply_box.box_size = 0;
        reply_box.n_publishers = 0;
        reply_box.n_subscribers = 0;

        if (write(man_pipe, &reply_box, sizeof(box_t)) < 1)
            exit(EXIT_FAILURE);
    } else {
        boxes[n_boxes - 1].last = 1;

        for (i = 0; i < n_boxes; i++) {
            reply_box = boxes[i];

            if (write(man_pipe, &reply_box, sizeof(box_t)) < 1)
                exit(EXIT_FAILURE);
        }
        // Reseta o last para no segundo list não bloquear neste elemento
        boxes[n_boxes - 1].last = 0;
    }
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

    ssize_t bytes_read; //= read_pipe(register_pipe, &code, sizeof(uint8_t));
    register_request_t box_request;
    thread_args args;
    size_t index;
    while ((bytes_read = read_pipe(*register_pipe, &code, sizeof(uint8_t))) !=
           -1) {
        // if(bytes_read > 0){
        // }
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
            if (pcq_enqueue(queue, &tid[index]) == 0) {

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
            if (pcq_enqueue(queue, &tid[index]) == 0) {

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

            if (pcq_enqueue(queue, &tid[index]) == 0) {

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
            if (pcq_enqueue(queue, &tid[index]) == 0) {

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

            if (pcq_enqueue(queue, &tid[index]) == 0) {

                if (pthread_create(&tid[index], NULL, list_boxes,
                                   (void *)&pipe_name) != 0) {
                    exit(EXIT_FAILURE);
                }
            }
            break;
        default:
            break;
        }
        code = 0;
    }
    return NULL;
}

int main(int argc, char **argv) {
    // Can't catch SIGINT
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        exit(EXIT_FAILURE);
    

    (void)argc;

    tfs_init(NULL);
    n_boxes = 0;

    char *register_pipe_name = argv[1];

    long max_s = strtol(argv[2], NULL, 0);

    max_sessions = (size_t)max_s;

    tid = malloc(sizeof(pthread_t) * max_sessions);

    queue = (pc_queue_t *)malloc(sizeof(pc_queue_t));
    pcq_create(queue, max_sessions);
    
    pthread_t main_thread;

    // Cria register_pipe
    if (!create_pipe(register_pipe_name))
        return -1;

    int register_pipe = open_pipe(register_pipe_name, 'r');

    if (pthread_create(&main_thread, NULL, main_thread_function,
                       (void *)&register_pipe) != 0) {
        fprintf(stderr, "[ERR]: Fail to create main thread: %s\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    pthread_join(main_thread, NULL);

    mbroker_close();

    printf("\nMbroker fechou\n\n");
    return 0;
}
