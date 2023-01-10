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

    int register_int = open_pipe(register_pipe, 'w');

    if( !create_pipe(pub_pipename) || !register_int)
        return -1;

    uint8_t code = 1;
    register_request_t request;

    strcpy(request.client_name_pipe_path, pub_pipename);
    strcpy(request.box_name, box_name);

    if(write(register_int, &code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
    if(write(register_int, &request, sizeof(register_request_t)) < 1)
            exit(EXIT_FAILURE);
    printf("PEDIU O REGISTO DE PUBLISHER\n");

    // registou !!
    char str[MESSAGE_SIZE];
    char* message = "";

    int tx = open_pipe(pub_pipename, 'w');
    if(tx == -1)
        return -1;


    while(fgets(str, MESSAGE_SIZE, stdin) != NULL){
        
        strcpy(message, str);
        fill_string(MESSAGE_SIZE, message);

        // TODO: Envia mensagem pelo pipe ao server
        code = 9;

        if(write(tx, &code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
        if(write(tx, &str, sizeof(char)*MESSAGE_SIZE) < 1)
            exit(EXIT_FAILURE);
    }

    // TODO: Envia mensagem pelo pipe ao server

    close(tx);

    return -1;
}
