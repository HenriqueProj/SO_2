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

// Maximo de boxes, equivalente ao numero maximo de inodes do tfs
#define MAX_BOXES 64

box_t boxes[MAX_BOXES];
int n_boxes;

void box_swap(int i){
    char* string_aux = "";
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

void delete_box(box_t box){
    char* box_name;
    for(int i = 0; i < n_boxes; i++){
        box_name = boxes[i].box_name;

        if( !strcmp(box_name, box.box_name) ){
            box_swap(i);
        }
    }
}

bool register_publisher(char* client_named_pipe_path, box_t box){

    char* box_name = box.box_name;

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    int box_handle = find_in_dir(root_dir_inode, box_name + 1);
    
    // Box não existe
    if( box_handle == -1)
        return false;
    
    // Há um publisher na box
    if(box.publisher != NULL)
        return false;
    
    // Nao consegue criar o pipe do publisher 
    if(!create_pipe(client_named_pipe_path) )
        return false;

    // Success!!
    strcpy(box.publisher, client_named_pipe_path);
    box.n_publishers++;

    return true;
}

bool register_subscriber(char* client_named_pipe_path, box_t box){
    char* box_name = box.box_name;

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    int box_handle = find_in_dir(root_dir_inode, box_name + 1);
    
    // Box não existe
    if( box_handle == -1)
        return false;
    
    // Nao consegue criar o pipe do publisher 
    if(!create_pipe(client_named_pipe_path) )
        return false;

    // Success!!
    box.n_subscribers++;

    return true;
}

void reply_to_box_creation(char* pipe_name, int n) {
    
    box_reply_t reply;
    uint8_t code = 4;

    reply.return_code = 0;
    fill_string(MESSAGE_SIZE, reply.error_message);
    //error creating box
    if(n == 0) {
        reply.return_code = -1;
        strcpy(reply.error_message, "ERROR: Couldn't create box");
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
    }
    int file_handle = find_in_dir(inode_get(ROOT_DIR_INUM), box_request.box_name + 1);
    
    //the box already exists
    if(file_handle != -1){
        reply_to_box_creation(box_request.client_name_pipe_path, 0);
    }
    int box_handle = tfs_open(box_request.box_name, TFS_O_CREAT);

    if(box_handle == -1){
        reply_to_box_creation(box_request.client_name_pipe_path, 0);
    }

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
        strcpy(reply.error_message, "ERROR: Could'nt create box");
    } 

   int manager_pipe = open_pipe(pipe_name, 'w');


    if(write(manager_pipe, &code, sizeof(uint8_t)) < 1){
        exit(EXIT_FAILURE);
    }
    if(write(manager_pipe, &reply, sizeof(box_reply_t)) < 1){
        exit(EXIT_FAILURE);
    }
}

void remove_box(int register_pipe) {
    box_t box;
    register_request_t box_request;
    printf("1\n");
    ssize_t bytes_read = read_pipe(register_pipe, &box_request, sizeof(register_request_t));

    if(bytes_read == -1)
        reply_to_box_removal(box_request.client_name_pipe_path, 0);
    printf("1\n");
    //verificar se está no array global??

    //the box doesn't exist
    if(find_in_dir(inode_get(ROOT_DIR_INUM), box_request.box_name) == -1)
        reply_to_box_removal(box_request.client_name_pipe_path, 0);
    printf("2\n");
    //unlink file associated to box
    int box_handle = tfs_unlink(box_request.box_name);

    if(box_handle == -1) 
        reply_to_box_removal(box_request.client_name_pipe_path, 0);
    printf("3\n");
    //free box from array
    delete_box(box);
    printf("4\n");
    reply_to_box_removal(box_request.client_name_pipe_path, 1);

}

int main(int argc, char **argv) {
    (void)argc;
    
    tfs_init(NULL);
    n_boxes = 0;

    char* register_pipe_name = argv[1];
    //int max_sessions = (int) argv[2];
    //pthread_t tid[max_sessions];

    // Cria register_pipe
    if(!create_pipe(register_pipe_name))
        return -1;

    int register_pipe = open_pipe(register_pipe_name, 'r'); 

    uint8_t code;

    ssize_t bytes_read; //= read_pipe(register_pipe, &code, sizeof(uint8_t));

    while((bytes_read = read_pipe(register_pipe, &code, sizeof(uint8_t))) != -1) {
        switch(code) {
        case 1:
                //register publisher
                break;
        case 2:
                //register subscriber
                break;
        case 3:
                //create box
                create_box(register_pipe);
                break;
        case 4:
                //reply to box creation
                // is this case neeeded??
                break;
        case 5:
                //box removal
                remove_box(register_pipe);
                break;
        case 6:
                //reply to box removal
                // is this case needed?
                break;
        case 7:
                //list boxes
                break;
        case 8:
                //reply to list the boxes
                break;
        case 9:
                //messages sent from publisher to server
                break;
        case 10:
                //messages sent from server to subscriber
                break;
        default:
                //should generate error?
                break;
        }
    }        

    fprintf(stderr, "usage: mbroker <pipename>\n");
    return 0;
}
