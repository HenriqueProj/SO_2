#include "logging.h"
#include <errno.h>
#include <stdbool.h>
#include <utils.h>
#include <string.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

int main(int argc, char **argv) {
    (void)argc;
    
    char* register_pipe = argv[1];
    char* pub_pipename = argv[2];
    char* box_name = argv[3];

    fill_string(PIPE_NAME_SIZE, pub_pipename);
    fill_string(BOX_NAME_SIZE, box_name);

    if( !create_pipe(pub_pipename) || !open_pipe(register_pipe, 'w'))
        return -1;

    // TODO: Envia o pedido de registo
    //...
    
    // registou !!
    char* str;
    char* message = "";

    int tx = open_pipe(pub_pipename, 'w');
    if(tx == -1)
        return -1;


    while(fgets(str, MESSAGE_SIZE, stdin) != NULL){
        
        strcpy(message, str);
        fill_string(MESSAGE_SIZE, message);

        // TODO: Envia mensagem pelo pipe ao server
    }

    // TODO: Envia mensagem pelo pipe ao server

    close(tx);

    return -1;
}
