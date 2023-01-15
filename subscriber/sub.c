#include "logging.h"
#include <errno.h>
#include <fcntl.h> // for open
#include <signal.h>
#include <string.h>
#include <unistd.h> // for close
#include <utils.h>

int n_messages = 0;
char *sub_name;

// Dá print da mensagem após ser sinalizado o final da sessão
static void handle() {

    fprintf(stdout, "Subscriber %s recebeu %d mensagens\n", sub_name,
            n_messages);
    exit(1);
}

// Rotina assíncrona para lidar com o sinal SIGINT(Ctrl+C)
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
    // não tem argumentos suficientes
    if (argc < 4)
        return -1;

    char *register_pipe = argv[1];
    char *sub_pipename = argv[2];
    char *box_name = argv[3];

    sub_name = argv[2];

    // abre para escrita, para fazer o pedido de registo
    int register_int = open_pipe(register_pipe, 'w');

    // verifica se há erro ao abrir o pipe ou ao criar o seu próprio pipe
    if (!create_pipe(sub_pipename) || !register_int)
        return -1;

    // código para o registo de subscritor
    uint8_t code = 2;
    register_request_t request;

    // guarda os elementos do subscritor na estrutura request
    strcpy(request.client_name_pipe_path, sub_pipename);
    strcpy(request.box_name, box_name);

    // preenche o resto das strings com '\0'
    fill_string(PIPE_NAME_SIZE, request.client_name_pipe_path);
    fill_string(BOX_NAME_SIZE, request.box_name);

    // manda o código de pedido
    if (write(register_int, &code, sizeof(uint8_t)) < 1)
        exit(EXIT_FAILURE);
    // mando a estrutura que representa o subscritor
    if (write(register_int, &request, sizeof(register_request_t)) < 1)
        exit(EXIT_FAILURE);

    // abre o seu pipe para leitura, para receber mensagens
    int tx = open_pipe(sub_pipename, 'r');

    // caso haja erro ao abrir o seu pipe
    if (tx == 0)
        return -1;

    char message[MESSAGE_SIZE];
    ssize_t bytes_read = read_pipe(tx, &code, sizeof(uint8_t));

    // Só sai em caso de erro do read ou SIGINT
    // Ou seja mandado uma mensagem com código errado
    while (bytes_read != -1) {
        // lê a mensagem
        read_pipe(tx, &message, MESSAGE_SIZE);

        if (bytes_read > 0 && code == 10) {
            // Imprime a mensagem e reseta o buffer
            fprintf(stdout, "%s\n", message);
            memset(message, 0, MESSAGE_SIZE);
            n_messages++;
        }

        // Checka o SIGINT
        if (signal(SIGINT, sig_handler) == SIG_ERR)
            return -1;
        bytes_read = read_pipe(tx, &code, sizeof(uint8_t));
    }

    // Saiu do loop por erro
    fprintf(stderr, "[ERR]: READ FAILED: %s\n", strerror(errno));
    printf("%ld %d\n%s\n", bytes_read, code, message);
    return 0;
}
