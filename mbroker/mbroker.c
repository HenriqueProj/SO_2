#include "logging.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <operations.h>
#include <state.h>
#include <utils.h>
#include "config.h"

box_t boxes[MAX_BOXES];
size_t n_boxes;
int n_active_threads;
pthread_t* tid;
size_t max_sessions;

void delete_box(int i){
    char string_aux[PIPE_NAME_SIZE];
    uint64_t int_aux;
    uint8_t last_aux;

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

void add_box(box_t box){
    boxes[n_boxes] = box;
    n_boxes++;
}

int find_box(char* box_name) {
    char* name;
    for(int i = 0; i < n_boxes; i++){
        name = boxes[i].box_name;

        if(!strcmp(name, box_name) ){
            return i;
        }
    }
    return -1;

}

void* write_messages(void* arg){

    pub_args_t* args = (pub_args_t*)arg;

    char* box_name = args->box_name;
    
    char* pipename = args->pipename;

    int box_handle = tfs_open(box_name, TFS_O_APPEND);

    uint8_t code;
    char message[MESSAGE_SIZE];

    int pub_pipe = open_pipe(pipename, 'r');

    while((read_pipe(pub_pipe, &code, sizeof(uint8_t))) > 0) {
        read_pipe(pub_pipe, &message, MESSAGE_SIZE);
        message[strlen(message)] = '\0';

        tfs_write(box_handle, message, strlen(message)); 
        tfs_write(box_handle, "\0", sizeof(char));
        printf("%s\n", message);

    }
    tfs_close(box_handle);
    close(pub_pipe);

    return NULL;
}

void register_publisher(int register_pipe){
    register_request_t request;
    read_pipe(register_pipe, &request, sizeof(register_request_t));

    char box_name[BOX_NAME_SIZE + 1];
    box_name[0] = '/';
    strcpy(box_name+1, request.box_name);

    int box_handle = tfs_open(box_name, TFS_O_APPEND);
  
    // Box não existe
    if(box_handle == -1)
        return;
    
    int box_index = find_box(request.box_name);

    if(box_index == -1)
        return;

    // Para passar string para a função do publisher
    // Deste modo, passa um char**, em vez de ter de criar uma struct args{int, char*}
    tfs_close(box_handle);

    // Success!!
    boxes[box_index].n_publishers = 1;

    if(n_active_threads >= max_sessions ){
        printf("Too many threads!!!\n");
        return;
    }

    // Caso o numero de active threads mude durante a duração do publisher, o indice não altera
    int index = n_active_threads;
    n_active_threads++;
    
    pub_args_t args;

    strcpy(args.box_name, box_name);

    strcpy(args.pipename, request.client_name_pipe_path);

    if(pthread_create(&tid[index], NULL, write_messages, (void*)&args ) != 0){
        fprintf(stderr, "[ERR]: Fail to create publisher thread: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pthread_join(tid[index], NULL);
}


void* read_messages(void* args) {
    register_request_t* subscriber_request = (register_request_t*)args;

    char box_name[BOX_NAME_SIZE];
    box_name[0] = '/';
    strcpy(box_name+1, subscriber_request->box_name);
    int box_handle = tfs_open(box_name, 0);
    int subscriber_pipe = open_pipe(subscriber_request->client_name_pipe_path, 'w');

    if(box_handle == -1 || subscriber_pipe == -1) {
        return NULL;
    }

    char char_buffer;
    char buffer[MESSAGE_SIZE];
    int i = 0;
    ssize_t bytes_read = tfs_read(box_handle, &char_buffer, sizeof(char));
    while(bytes_read > 0) {
        if(char_buffer == '\0') {
            buffer[i] = '\0';
            fill_string(MESSAGE_SIZE, buffer);
            if(write(subscriber_pipe, &buffer, MESSAGE_SIZE) < 1){
                exit(EXIT_FAILURE);
            }
            //reset buffer e indíce
            memset(buffer, 0, MESSAGE_SIZE);
            i = 0;
        }
        else {
            buffer[i] = char_buffer;
            i++;
        }
        bytes_read = tfs_read(box_handle, &char_buffer, sizeof(char));
    }

    tfs_close(box_handle);

    return NULL;
}

void register_subscriber(int register_pipe){
    register_request_t subscriber_request;
    ssize_t bytes_read = read_pipe(register_pipe, &subscriber_request, sizeof(register_request_t));
    
    if(bytes_read == -1){
        return;
    }

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    int box_handle = find_in_dir(root_dir_inode, subscriber_request.box_name);
    
    // Box não existe
    if( box_handle == -1) {
        return;
    }

    int box_index = find_box(subscriber_request.box_name);

    if(box_index == -1)
        return;

    boxes[box_index].n_subscribers++;

    if(n_active_threads >= max_sessions ){
        printf("Too many threads!!!\n");
        return;
    }
    printf("%d %ld\n", n_active_threads, max_sessions);

    int index = n_active_threads;
    n_active_threads++;

    //Subscriber created sucessfully, will wait for messages to be written in message box
    if(pthread_create(&tid[index], NULL, read_messages, (void*)&subscriber_request ) != 0){
        fprintf(stderr, "[ERR]: Fail to create subscriber thread: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return;
}

void* reply_to_box_creation(void* arg) {
    manager_args_t* args = (manager_args_t*)arg;

    box_reply_t reply;
    uint8_t code = 4;

    reply.return_code = 0;
    fill_string(MESSAGE_SIZE, reply.error_message);
    //error creating box
    if(args->mode == 0) {
        reply.return_code = -1;
        strcpy(reply.error_message, "Couldn't create box");
    }
    
    int manager_pipe = open_pipe(args->pipename, 'w');


    if(write(manager_pipe, &code, sizeof(uint8_t)) < 1){
        exit(EXIT_FAILURE);
    }
    if(write(manager_pipe, &reply, sizeof(box_reply_t)) < 1){
        exit(EXIT_FAILURE);
    }
    close(manager_pipe);

    return NULL;
}

void create_box(int register_pipe) {
    box_t box;
    register_request_t box_request;
    ssize_t bytes_read = read_pipe(register_pipe, &box_request, sizeof(register_request_t));
    
    manager_args_t args;
    args.mode = 1;
    strcpy(args.pipename, box_request.client_name_pipe_path);

    if(bytes_read == -1){
        args.mode = 0;
    }
    int file_handle = find_in_dir(inode_get(ROOT_DIR_INUM), box_request.box_name);
    
    //the box already exists
    if(file_handle != -1){
        args.mode = 0;
    }

    for(int cont = 0; cont < n_boxes; cont++){
        if(!strcmp(boxes[cont].box_name, box_request.box_name) ){
            args.mode = 0;
        }
    }

    char box_name[BOX_NAME_SIZE];

    box_name[0] = '/';
    strcpy(box_name + 1, box_request.box_name);

    int box_handle = tfs_open(box_name, TFS_O_CREAT);

    if(box_handle == -1){
        args.mode = 0;
    }

    if(n_active_threads >= max_sessions ){
        printf("Too many threads!!!\n");
        tfs_close(box_handle);
        return;
    }

    // Caso o numero de active threads mude durante a duração do publisher, o indice não altera
    int index = n_active_threads;
    n_active_threads++;

    if(args.mode == 1){
        box.box_size = 0;
        box.n_publishers = 0;
        box.n_subscribers = 0;
        strcpy(box.box_name, box_request.box_name);
    }
    tfs_close(box_handle);

    if(pthread_create(&tid[index], NULL, reply_to_box_creation, (void*)&args ) != 0){
        fprintf(stderr, "[ERR]: Fail to create publisher thread: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    //adicionar box a um array de boxes global
    add_box(box);

}

void* reply_to_box_removal(void* arg) {
    manager_args_t* args = (manager_args_t*) arg;

    box_reply_t reply;
    
    uint8_t code = 6;
    reply.return_code = 0;

    fill_string(MESSAGE_SIZE, reply.error_message);
    //error creating box
    if(args->mode == 0) {
        reply.return_code = -1;
        strcpy(reply.error_message, "Couldn't remove box");
    } 

   int manager_pipe = open_pipe(args->pipename, 'w');


    if(write(manager_pipe, &code, sizeof(uint8_t)) < 1){
        printf("Writing error\n");
        exit(EXIT_FAILURE);
    }
    if(write(manager_pipe, &reply, sizeof(box_reply_t)) < 1){
        printf("Writing error\n");
        exit(EXIT_FAILURE);
    }

    return NULL;
}

void remove_box(int register_pipe) {
    register_request_t box_request;
    
    ssize_t bytes_read = read_pipe(register_pipe, &box_request, sizeof(register_request_t));

    manager_args_t args;
    args.mode = 1;
    strcpy(args.pipename, box_request.client_name_pipe_path);

    if(bytes_read == -1){
        args.mode = 0;
    }

    //the box doesn't exist
    //if(find_in_dir(inode_get(ROOT_DIR_INUM), box_request.box_name) == -1){
    //    reply_to_box_removal(box_request.client_name_pipe_path, 0);
    //    return;
    //}
    //unlink file associated to box
    int box_handle = tfs_unlink(box_request.box_name);

    if(box_handle == -1){
        args.mode = 0;
    }

    //verifica se está no array global
    int ver = -1;

    for(int cont = 0; cont < n_boxes; cont++){
        if(!strcmp(boxes[cont].box_name, box_request.box_name)){
            // Índice da box no array
            ver = cont;
            break;
        }
    }
    // Box não está no array global
    if(ver == -1){
        args.mode = 0;
    }

    if(n_active_threads >= max_sessions ){
        printf("Too many threads!!!\n");
        return;
    }

    // Caso o numero de active threads mude durante a duração do publisher, o indice não altera
    int index = n_active_threads;
    n_active_threads++;

    if(args.mode == 1){
        //free box from array
        printf("%s %s\n", box_request.box_name, boxes[ver].box_name);
        delete_box(ver);
    }

    if(pthread_create(&tid[index], NULL, reply_to_box_removal, (void*)&args ) != 0){
        fprintf(stderr, "[ERR]: Fail to create publisher thread: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

}

void* reply_to_list_boxes(void* args){
    char** manager_pipe = (char**)args;
    uint8_t code = 8;
    box_t reply_box;

    int man_pipe = open_pipe(*manager_pipe, 'w');

    int i = 0;

    if(write(man_pipe, &code, sizeof(uint8_t)) < 1){
        exit(EXIT_FAILURE);
    }
    if(n_boxes == 0){
        reply_box.last = 1;
        //fill_string(PIPE_NAME_SIZE, reply_box.publisher);
        //fill_string(PIPE_NAME_SIZE, reply_box.box_name);
        strcpy(reply_box.publisher, "\0");
        strcpy(reply_box.box_name, "\0");

        reply_box.box_size = 0;
        reply_box.n_publishers = 0;
        reply_box.n_subscribers = 0;

        if(write(man_pipe, &reply_box, sizeof(box_t)) < 1)
            exit(EXIT_FAILURE);
    }
    else{
        boxes[n_boxes - 1].last = 1;

        for(i = 0; i < n_boxes; i++){
            reply_box = boxes[i];
        

        if(write(man_pipe, &reply_box, sizeof(box_t)) < 1)
            exit(EXIT_FAILURE);
        }
        // Reseta o last para no segundo list não bloquear neste elemento
        boxes[n_boxes - 1].last = 0;
    }
    return NULL;
}

void list_boxes(int register_pipe){
    char manager_pipe[PIPE_NAME_SIZE];

    read_pipe(register_pipe, &manager_pipe, PIPE_NAME_SIZE);
    
    if(n_active_threads >= max_sessions ){
        printf("Too many threads!!!\n");
        return;
    }

    int index = n_active_threads;
    n_active_threads++;

    if(pthread_create(&tid[index], NULL, reply_to_list_boxes, (void*)&manager_pipe ) != 0){
        fprintf(stderr, "[ERR]: Fail to create publisher thread: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void publisher_messages(int register_pipe){
    char pub_pipe[PIPE_NAME_SIZE];

    read_pipe(register_pipe, &pub_pipe, PIPE_NAME_SIZE);
}

void* main_thread_function(void* arg){
    int* register_pipe = (int*)arg;

    uint8_t code;

    ssize_t bytes_read; //= read_pipe(register_pipe, &code, sizeof(uint8_t));
   
    while((bytes_read = read_pipe(*register_pipe, &code, sizeof(uint8_t))) != -1) {
        //if(bytes_read > 0){
        //}
        switch(code) {
        case 1:
                //register publisher
                register_publisher(*register_pipe);
                break;
        case 2:
                //register subscriber
                register_subscriber(*register_pipe);
                break;
        case 3:
                //create box
                create_box(*register_pipe);
                break;
        case 5:
                //box removal
                remove_box(*register_pipe);
                break;
        case 7:
                list_boxes(*register_pipe);
                break;
        case 9:
                //messages sent from publisher to server
                publisher_messages(*register_pipe);
                break;
        case 10:
                //messages sent from server to subscriber
                break;
        default:
                break;
        }
        code = 0;
    }
    return NULL;
}

int main(int argc, char **argv) {
    (void)argc;
    
    tfs_init(NULL);
    n_boxes = 0;
    n_active_threads = 0;

    char* register_pipe_name = argv[1];
    long max_s = strtol(argv[2], NULL, 0);

    printf("%ld", max_s);

    max_sessions = (size_t)max_s;

    tid = malloc( sizeof(pthread_t) * max_sessions);

    pthread_t main_thread;

    // Cria register_pipe
    if(!create_pipe(register_pipe_name))
        return -1;

    int register_pipe = open_pipe(register_pipe_name, 'r'); 

    if(pthread_create(&main_thread, NULL, main_thread_function, (void*)&register_pipe ) != 0){
        fprintf(stderr, "[ERR]: Fail to create main thread: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pthread_join(main_thread, NULL);

    fprintf(stderr, "usage: mbroker <pipename>\n");
    return 0;
}
