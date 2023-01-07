#include "logging.h"
#include <utils.h>

static void sig_handler(int sig, char* sub_name, int messages) {

    if (sig != SIGINT) 
        return;

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        return;
    }
    // FIXME : fprintf not safe em signal handlers
    //       : formato de mensagem não especificado no enunciado?????
    fprintf(stderr, "Subscriber %s recebeu %d mensagens\n", sub_name, messages);
    return;
}

int main(int argc, char **argv) {
    (void)argc;
    char* register_pipe = argv[0];
    char* sub_pipename = argv[1];
    char* box_name = argv[2];

    fill_string(PIPE_NAME_SIZE, sub_pipename);
    fill_string(BOX_NAME_SIZE, box_name);

    if( !create_pipe(sub_pipename) || !open_pipe(register_pipe, 'w'))
        return -1;

    // TODO: Envia o pedido de registo
    //...

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
            memset(message, "", MESSAGE_SIZE);
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
