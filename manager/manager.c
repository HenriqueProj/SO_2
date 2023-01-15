#include "logging.h"
#include <string.h>
#include <unistd.h>
#include <utils.h>

int main(int argc, char **argv) {

    char *register_pipe_name = argv[1];
    char *pipe_name = argv[2];
    char *type = argv[3];
    char *box_name;

    box_t boxes[MAX_BOXES];
    size_t n_boxes = 0;

    // caso tenha argumentos insuficientes
    if (argc < 4)
        return -1;

    // caso seja para criar ou remover uma caixa
    if (argc > 4) {
        box_name = argv[4];
    }

    // abre o pipe do servidor para mandar o pedido de registo
    int register_pipe = open_pipe(register_pipe_name, 'w');

    // verifica se há erros ao criar o manager pipe ou a abrir o register pipe
    if (!create_pipe(pipe_name) || register_pipe == -1)
        return -1;

    uint8_t request_code;

    // Pedido de criação de caixa
    if (!strcmp(type, "create")) {
        // código para a criação de uma caixa
        request_code = 3;

        register_request_t request;
        strcpy(request.client_name_pipe_path, pipe_name);
        strcpy(request.box_name, box_name);
        // preenche o resto das strings com '\0'
        fill_string(PIPE_NAME_SIZE, request.client_name_pipe_path);
        fill_string(BOX_NAME_SIZE, request.box_name);
        printf("box_name:%s, pipe_name:%s\n", request.box_name,
               request.client_name_pipe_path);
        // manda o pedido para o servidor
        if (write(register_pipe, &request_code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
        if (write(register_pipe, &request, sizeof(register_request_t)) < 1)
            exit(EXIT_FAILURE);

        close(register_pipe);
    }
    // Pedido de remoção de caixa
    else if (!strcmp(type, "remove")) {
        // ccódigo para remoção de uma caixa
        request_code = 5;
        register_request_t request;

        strcpy(request.client_name_pipe_path, pipe_name);
        strcpy(request.box_name, box_name);

        // preenche o resto das strigs com '\0'
        fill_string(PIPE_NAME_SIZE, request.client_name_pipe_path);
        fill_string(BOX_NAME_SIZE, request.box_name);
        printf("box_name:%s, pipe_name:%s\n", request.box_name,
               request.client_name_pipe_path);
        // manda o pedido para o servidor
        if (write(register_pipe, &request_code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
        if (write(register_pipe, &request, sizeof(register_request_t)) < 1)
            exit(EXIT_FAILURE);

        close(register_pipe);
    }
    // Pedido de listagem de caixas
    else if (!strcmp(type, "list")) {
        // código de pedido de listagem das caixas
        request_code = 7;

        // Evita um string overread no read
        // Por causa do \0, assume que o tamanho máximo do ponteiro é o strlen
        // do ponteiro
        char named_pipe[PIPE_NAME_SIZE];
        strcpy(named_pipe, pipe_name);

        fill_string(PIPE_NAME_SIZE, named_pipe);

        // manda o pedido de listagem das caixas para o servidor
        if (write(register_pipe, &request_code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
        if (write(register_pipe, &named_pipe, PIPE_NAME_SIZE) < 1)
            exit(EXIT_FAILURE);

        close(register_pipe);
    }

    // abre o pipe da manager para leitura
    int tx = open_pipe(pipe_name, 'r');
    if (tx == -1)
        return -1;

    uint8_t code;

    // lê o código da resposta do servidor ao manager
    read_pipe(tx, &code, sizeof(uint8_t));

    // Resposta ao pedido de criação ou remoção
    if (code == 4 || code == 6) {
        box_reply_t box_reply;

        read_pipe(tx, &box_reply, sizeof(box_reply_t));

        // fecha o seu named pipe porque já recebeu a resposta
        close(tx);

        // em caso de erro
        if (box_reply.return_code == -1) {
            fprintf(stdout, "ERROR: %s\n", box_reply.error_message);
            return -1;
        }

        fprintf(stdout, "OK\n");
        // Caixa e struct_box foram criadas / removidas pelo mbroker
    }
    // Resposta ao pedido de Listagem das caixas
    else if (code == 8) {

        box_t box;

        read_pipe(tx, &box, sizeof(box_t));

        // caso não haja caixas no servidor
        if (box.last == 1 && box.box_name[0] == '\0') {
            fprintf(stdout, "NO BOXES FOUND\n");
            return -1;
        }
        boxes[0] = box;
        n_boxes++;

        // guarda-se as caixas num array
        while (box.last != 1) {
            read_pipe(tx, &box, sizeof(box_t));
            boxes[n_boxes] = box;
            n_boxes++;
        }

        // fecha-se o pipe porque já se leram todas as caixas
        close(tx);

        // caso haja mais que uma caixa para listar, ordenam-se as caixas por
        // ordem alfabética
        if (n_boxes > 1)
            qsort(&boxes[0], n_boxes, sizeof(box_t), &compare_structs);

        // listagem das caixas
        for (int i = 0; i < n_boxes; i++)
            fprintf(stdout, "%s %zu %zu %zu\n", boxes[i].box_name,
                    boxes[i].box_size, boxes[i].n_publishers,
                    boxes[i].n_subscribers);
    } else
        return -1;

    return 0;
}
