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
#include <pipes.h>

bool register_publisher(char* client_named_pipe_path, char* box_name){
    char buffer[1];

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    int box_handle = find_in_dir(root_dir_inode, box_name);
    
    // Box não existe
    if( box_handle == -1)
        return false;
    
    // Box não vazia - há um publisher na box já (ou a box não se apagou depois de dar kill no publisher)
    // FIXME: Solução probably errada
    if(tfs_read(box_handle, buffer, 1) != 0)
        return false;
    
    if(!create_pipe(client_named_pipe_path) )
        return false;

    // Success!!
    return true;
}
/*
bool register_sub(char* client_named_pipe_path, char* box_name){
}
bool create_box(char* client_named_pipe_path, char* box_name){
}
bool remove_box(char* client_named_pipe_path, char* box_name){
}
bool list_boxes(char* client_named_pipe_path){
}
*/
int main(int argc, char **argv) {
    (void)argc;
    
    char* register_pipe = argv[0];
    //int max_sessions = (int) argv[1];
    //pthread_t tid[max_sessions];

    // Cria register_pipe
    if(!create_pipe(register_pipe))
        return -1;


    fprintf(stderr, "usage: mbroker <pipename>\n");
    return 0;
}
