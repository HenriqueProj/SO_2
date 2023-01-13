#include "logging.h"
#include <utils.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close


int n_messages = 0;
char* sub_name;

static void handle() {

    fprintf(stdout, "Subscriber %s recebeu %d mensagens\n", sub_name, n_messages);
    exit(1);
}

static void sig_handler(int sig) {
    if (sig != SIGINT) 
        return;

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        return;
    }
    handle();
    return;
}

int main(int argc, char **argv) {
    (void)argc;
    char* register_pipe = argv[1];
    char* sub_pipename = argv[2];
    char* box_name = argv[3];
    
    sub_name = argv[2];


    int register_int = open_pipe(register_pipe, 'w');

    if( !create_pipe(sub_pipename) || !register_int)
        return -1;

    uint8_t code = 2;
    register_request_t request;

    strcpy(request.client_name_pipe_path, sub_pipename);
    strcpy(request.box_name, box_name);

    fill_string(PIPE_NAME_SIZE, request.client_name_pipe_path);
    fill_string(BOX_NAME_SIZE, request.box_name);

    if(write(register_int, &code, sizeof(uint8_t)) < 1)
        exit(EXIT_FAILURE);
    if(write(register_int, &request, sizeof(register_request_t)) < 1)
        exit(EXIT_FAILURE);
        
    // registou !!
    int tx = open_pipe(sub_pipename, 'r');
    if(tx == 0)
        return -1;

    char message[MESSAGE_SIZE];

    ssize_t bytes_read = read_pipe(tx, &message, MESSAGE_SIZE);

    // Só sai em caso de erro do read ou SIGINT
    // FIXME : Espera ativa?
    while(bytes_read != -1){

        // Caso ainda não tenha lido tudo
        if(bytes_read > 0){
            // Imprime a mensagem e reseta o buffer
            fprintf(stdout, "%s\n", message);
            memset(message, 0, MESSAGE_SIZE);
            n_messages++;
        } 

        // Checka o SIGINT
        if (signal(SIGINT, sig_handler) == SIG_ERR) 
            return -1;

        bytes_read = read_pipe(tx, &message, MESSAGE_SIZE);
    }


    // Saiu do loop por erro
    fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
    return 0;
}
