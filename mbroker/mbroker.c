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
    box.publisher = client_named_pipe_path;
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

void reply_to_box_creation(int n) {
    (void) n;
    /*box_reply_t reply;
    
    reply.code = 4;
    reply.return_code = 0;
    strcpy(reply.error_message, '\0');
    //error creating box
    if(n == 0) {
        reply.return_code = -1;
        strcpy(reply.error_message, "ERROR: Could'nt create box");
    }*/

    //send reply to manager

}

void create_box(int register_pipe) {
    //box_t box;
    register_request_t box_request;
    ssize_t bytes_read = read_pipe(register_pipe, &box_request, sizeof(register_request_t));

    if(bytes_read == -1)
        reply_to_box_creation(0);
    
    int file_handle = find_in_dir(inode_get(ROOT_DIR_INUM), box_request.box_name + 1);
    
    //the box already exists
    if(file_handle != -1)
        reply_to_box_creation(0);
    
    int box_handle = tfs_open(box_request.box_name, TFS_O_CREAT);

    if(box_handle == -1) 
        reply_to_box_creation(0);

    /*box.n_publishers = 0;
    box.n_subscribers = 0;
    strcpy(box.box_name, box_request.box_name);*/
    

    tfs_close(box_handle);
    reply_to_box_creation(1);
    //adicionar box a um array de boxes global
}

void reply_to_box_removal(int n) {
    (void) n;
    /*box_reply_t reply;
    
    reply.code = 6;
    reply.return_code = 0;
    strcpy(reply.error_message, '\0');
    //error creating box
    if(n == 0) {
        reply.return_code = -1;
        strcpy(reply.error_message, "ERROR: Could'nt create box");
    } */

    //send reply to manager
}

void remove_box(int register_pipe) {
    //box_t box;
    register_request_t box_request;
    ssize_t bytes_read = read_pipe(register_pipe, &box_request, sizeof(register_request_t));

    if(bytes_read == -1)
        reply_to_box_removal(0);
    
    //verificar se está no array global??

    //the box doesn't exist
    if(find_in_dir(inode_get(ROOT_DIR_INUM), box_request.box_name) == -1)
        reply_to_box_removal(0);
    
    //unlink file associated to box
    int box_handle = tfs_unlink(box_request.box_name);

    if(box_handle == -1) 
        reply_to_box_removal(0);
    
    //free box from array
    
    reply_to_box_removal(1);

}


int main(int argc, char **argv) {
    (void)argc;
    
    char* register_pipe_name = argv[1];
    //int max_sessions = (int) argv[2];
    //pthread_t tid[max_sessions];

    // Cria register_pipe
    if(!create_pipe(register_pipe_name))
        return -1;

    int register_pipe = open_pipe(register_pipe_name, 'r'); 

    uint8_t code;

    ssize_t bytes_read = read_pipe(register_pipe, &code, sizeof(uint8_t));

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
