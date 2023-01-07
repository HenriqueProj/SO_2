#ifndef __UTILS_UTILS_H__
#define __UTILS_UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define PIPE_NAME_SIZE 256
#define BOX_NAME_SIZE 32
#define MESSAGE_SIZE 1024

typedef struct {
    char* box_name;
    char* publisher;


    uint64_t box_size;
    uint64_t n_publishers;
    uint64_t n_subscribers;

    uint8_t last;
} box_t;

// Structs para envio de pedidos por pipes

// Registo de publishers e subscribers
// Ou criação / remoção de caixas
typedef struct {
    uint8_t code;
    char* client_name_pipe_path;
    char* box_name;
} register_request_t;

// Resposta ao pedido de criação/remoção de caixa
typedef struct {
    uint8_t code;
    int32_t return_code;
    char* error_message;
} box_reply_t;

// Envio de mensagem do publicador para servidor / servidor para subscritor
// ou pedido de listagem de caixas
typedef struct {
    uint8_t code;
    char* string;
} message_exchange_t;

int create_pipe(char* pipename);
int open_pipe(char* pipename, char mode);
/*void write_pipe(int tx, char const *str);*/
void fill_string(int size, char* array);
int read_pipe(int rx, void* buffer, int size);

#endif