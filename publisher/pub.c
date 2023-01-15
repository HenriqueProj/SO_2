#include "logging.h"
#include <errno.h>
#include <fcntl.h> // for open
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h> // for close
#include <utils.h>

int tx;

static void sig_handler(int sig) {

    if (sig == SIGPIPE) {

        //
        if (signal(SIGPIPE, sig_handler) == SIG_ERR) {
            exit(EXIT_FAILURE);
        }
        close(tx);
    }

    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (signal(SIGPIPE, sig_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }
    // não tem argumentos suficientes
    if (argc < 4)
        return -1;

    char *register_pipe = argv[1];
    char *pub_pipename = argv[2];
    char *box_name = argv[3];

    // abre o register pipe para mandar o pedido de registo
    int register_int = open_pipe(register_pipe, 'w');

    // verifica se há erro ao abrir o pipe ou a criar o seu próprio pipe
    if (!create_pipe(pub_pipename) || !register_int)
        return -1;

    // código de registo de um publisher
    uint8_t code = 1;
    register_request_t request;

    // guarda os prarâmetros de registo na estrutura
    strcpy(request.client_name_pipe_path, pub_pipename);
    strcpy(request.box_name, box_name);

    // preenche o resto das strings com '\0'
    fill_string(PIPE_NAME_SIZE, request.client_name_pipe_path);
    fill_string(BOX_NAME_SIZE, request.box_name);

    // escreve o código e a estrutura com o pedido
    if (write(register_int, &code, sizeof(uint8_t)) < 1)
        exit(EXIT_FAILURE);
    if (write(register_int, &request, sizeof(register_request_t)) < 1)
        exit(EXIT_FAILURE);

    // fecha o pipe de registo
    close(register_int);

    char str[MESSAGE_SIZE];
    char message[MESSAGE_SIZE];

    // abre-se o pipe do publisher para escrita
    tx = open_pipe(request.client_name_pipe_path, 'w');

    if (tx == -1)
        return -1;

    // código para o envio de mensagens do publiher para o servidor
    code = 9;

    // Lê do stdin até receber um EOF ou CTRL+D
    while (fgets(str, MESSAGE_SIZE, stdin) != NULL) {
        strcpy(message, str);

        size_t len = strlen(message);

        // acrescenta '\0' no final da mensagem
        if (len == MESSAGE_SIZE)
            message[MESSAGE_SIZE - 1] = '\0';
        else if (len == 0)
            message[0] = '\0';
        else {
            // substitui o '\n' por '\0' e prrenche o resto da mensagem com '\0'
            message[len - 1] = '\0';
            fill_string(MESSAGE_SIZE, message);
        }

        // Envia mensagem pelo pipe ao server
        if (write(tx, &code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
        if (write(tx, &message, MESSAGE_SIZE) < 1)
            exit(EXIT_FAILURE);
    }

    close(tx);
    return -1;
}
