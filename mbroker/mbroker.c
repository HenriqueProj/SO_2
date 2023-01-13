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

void publisher_function(int register_pipe){
    register_request_t request;
    read_pipe(register_pipe, &request, sizeof(register_request_t));

    char box_name[BOX_NAME_SIZE];
    box_name[0] = '/';
    strcpy(box_name+1, request.box_name);

    int box_handle = tfs_open(box_name, TFS_O_APPEND);
  
    // Box não existe
    if(box_handle == -1)
        return;
    
    int box_index = find_box(request.box_name);

    if(box_index == -1)
        return;
        

    // Success!!
    boxes[box_index].n_publishers = 1;

    // FIM DO REGISTO
    uint8_t code;
    char message[MESSAGE_SIZE];

    int pub_pipe = open_pipe(request.client_name_pipe_path, 'r');

    while((read_pipe(pub_pipe, &code, sizeof(uint8_t))) > 0) {
        read_pipe(pub_pipe, &message, MESSAGE_SIZE);
        message[strlen(message)] = '\0';
        printf("from pipe:");
        tfs_write(box_handle, message, strlen(message)); 
        tfs_write(box_handle, "\0", sizeof(char));
        printf("%s", message);
        // FIXME : 
    }
    close(box_handle);
}

void read_messages(register_request_t subscriber_request) {
    char box_name[BOX_NAME_SIZE];
    box_name[0] = '/';
    strcpy(box_name+1, subscriber_request.box_name);
    int box_handle = tfs_open(box_name, 0);
    int subscriber_pipe = open_pipe(subscriber_request.client_name_pipe_path, 'w');

    if(box_handle == -1 || subscriber_pipe == -1) {
        return;
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

    close(box_handle);
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

    printf("\nSUBSCRIBER REGISTED\n");

    //Subscriber created sucessfully, will wait for messages to be written in message box
    read_messages(subscriber_request);

    return;
}

void reply_to_box_creation(char* pipe_name, int n) {
    
    box_reply_t reply;
    uint8_t code = 4;

    reply.return_code = 0;
    fill_string(MESSAGE_SIZE, reply.error_message);
    //error creating box
    if(n == 0) {
        reply.return_code = -1;
        strcpy(reply.error_message, "Couldn't create box");
    }
    
    int manager_pipe = open_pipe(pipe_name, 'w');


    if(write(manager_pipe, &code, sizeof(uint8_t)) < 1){
        exit(EXIT_FAILURE);
    }
    if(write(manager_pipe, &reply, sizeof(box_reply_t)) < 1){
        exit(EXIT_FAILURE);
    }
    close(manager_pipe);
}

void create_box(int register_pipe) {
    box_t box;
    register_request_t box_request;
    ssize_t bytes_read = read_pipe(register_pipe, &box_request, sizeof(register_request_t));
   
    if(bytes_read == -1){
        reply_to_box_creation(box_request.client_name_pipe_path, 0);
        return;
    }
    int file_handle = find_in_dir(inode_get(ROOT_DIR_INUM), box_request.box_name);
    
    //the box already exists
    if(file_handle != -1){
        reply_to_box_creation(box_request.client_name_pipe_path, 0);
        return;
    }
    for(int cont = 0; cont < n_boxes; cont++){
        if(!strcmp(boxes[cont].box_name, box_request.box_name) ){
            reply_to_box_creation(box_request.client_name_pipe_path, 0);
            return;
        }
    }
    char box_name[BOX_NAME_SIZE];

    box_name[0] = '/';
    strcpy(box_name + 1, box_request.box_name);

    int box_handle = tfs_open(box_name, TFS_O_CREAT);

    if(box_handle == -1){
        reply_to_box_creation(box_request.client_name_pipe_path, 0);
        return;
    }
    box.box_size = 0;
    box.n_publishers = 0;
    box.n_subscribers = 0;
    strcpy(box.box_name, box_request.box_name);
    
    tfs_close(box_handle);
    reply_to_box_creation(box_request.client_name_pipe_path, 1);

    //adicionar box a um array de boxes global
    add_box(box);

}

void reply_to_box_removal(char* pipe_name, int n) {
    box_reply_t reply;
    
    uint8_t code = 6;
    reply.return_code = 0;

    fill_string(MESSAGE_SIZE, reply.error_message);
    //error creating box
    if(n == 0) {
        reply.return_code = -1;
        strcpy(reply.error_message, "Couldn't remove box");
    } 

   int manager_pipe = open_pipe(pipe_name, 'w');


    if(write(manager_pipe, &code, sizeof(uint8_t)) < 1){
        printf("Writing error\n");
        exit(EXIT_FAILURE);
    }
    if(write(manager_pipe, &reply, sizeof(box_reply_t)) < 1){
        printf("Writing error\n");
        exit(EXIT_FAILURE);
    }
}

void remove_box(int register_pipe) {
    register_request_t box_request;
    
    ssize_t bytes_read = read_pipe(register_pipe, &box_request, sizeof(register_request_t));

    if(bytes_read == -1){
        reply_to_box_removal(box_request.client_name_pipe_path, 0);
        return;
    }
    printf("1\n");
    //the box doesn't exist
    //if(find_in_dir(inode_get(ROOT_DIR_INUM), box_request.box_name) == -1){
    //    reply_to_box_removal(box_request.client_name_pipe_path, 0);
    //    return;
    //}
    //unlink file associated to box
    int box_handle = tfs_unlink(box_request.box_name);

    if(box_handle == -1){
        reply_to_box_removal(box_request.client_name_pipe_path, 0);
        return;
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
        reply_to_box_removal(box_request.client_name_pipe_path, 0);
        return;
    }

    //free box from array
    printf("%s %s\n", box_request.box_name, boxes[ver].box_name);
    delete_box(ver);

    reply_to_box_removal(box_request.client_name_pipe_path, 1);

}

void reply_to_list_boxes(char* manager_pipe){
    uint8_t code = 8;
    box_t reply_box;

    int man_pipe = open_pipe(manager_pipe, 'w');

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
}

void list_boxes(int register_pipe){
    char manager_pipe[PIPE_NAME_SIZE];

    read_pipe(register_pipe, &manager_pipe, PIPE_NAME_SIZE);
    
    reply_to_list_boxes(manager_pipe);
}

void publisher_messages(int register_pipe){
    char pub_pipe[PIPE_NAME_SIZE];

    read_pipe(register_pipe, &pub_pipe, PIPE_NAME_SIZE);
}

void* main_thread_function(void* arg){
    int* register_pipe = (int*) arg;

    uint8_t code;

    ssize_t bytes_read; //= read_pipe(register_pipe, &code, sizeof(uint8_t));
   
    while((bytes_read = read_pipe(*register_pipe, &code, sizeof(uint8_t))) != -1) {
        //if(bytes_read > 0){
        //}
        switch(code) {
        case 1:
                //register publisher
                publisher_function(*register_pipe);
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

    char* register_pipe_name = argv[1];
    //int max_sessions = (int) argv[2];
    //pthread_t tid[max_sessions];

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
