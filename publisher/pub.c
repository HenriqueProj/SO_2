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

    int register_int = open_pipe(register_pipe, 'w');

    if( !create_pipe(pub_pipename) || !register_int)
        return -1;

    uint8_t code = 1;
    register_request_t request;

    strcpy(request.client_name_pipe_path, pub_pipename);
    strcpy(request.box_name, box_name);

    fill_string(PIPE_NAME_SIZE, request.client_name_pipe_path);
    fill_string(BOX_NAME_SIZE, request.box_name);

    if(write(register_int, &code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
    if(write(register_int, &request, sizeof(register_request_t)) < 1)
            exit(EXIT_FAILURE);

    close(register_int);

    // registou !!
    char str[MESSAGE_SIZE];
    char message[MESSAGE_SIZE];

    int tx = open_pipe(request.client_name_pipe_path, 'w');

    if(tx == -1)
        return -1;

    while(fgets(str, MESSAGE_SIZE, stdin) != NULL){
        strcpy(message, str);
  
        size_t len = strlen(message);

        if(len == MESSAGE_SIZE)
            message[MESSAGE_SIZE - 1] = '\0';
        else {
            message[strcspn(message, "\n")] = 0;;
        }
        // Envia mensagem pelo pipe ao server

        if(write(tx, &code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
        if(write(tx, &str, sizeof(char)*MESSAGE_SIZE) < 1)
            exit(EXIT_FAILURE);
    }

    close(tx);
    return -1;
}
