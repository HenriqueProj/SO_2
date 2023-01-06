#include "logging.h"
#include <errno.h>
#include <stdbool.h>
#include <utils.h>

int main(int argc, char **argv) {
    (void)argc;
    
    char* register_pipe = argv[0];
    char* pub_pipename = argv[1];
    char* box_name = argv[2];

    fill_string(PIPE_NAME_SIZE, pub_pipename);
    fill_string(BOX_NAME_SIZE, box_name);

    if( !create_pipe(pub_pipename) || !open_pipe(register_pipe, 'w'))
        return -1;

    // TODO: Envia o pedido de registo
    //...
    
    // registou !!
    char* str;

    tx = open_pipe(pub_pipename, 'w');
    if(tx == -1)
        return -1;

    while(fgets(str, MESSAGE_SIZE, stdin) != NULL){
        message += str;
    }

    // TODO: Envia mensagem pelo pipe ao server

    close(tx);

    return -1;
}
