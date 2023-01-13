#ifndef __UTILS_UTILS_H__
#define __UTILS_UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Maximo de boxes, equivalente ao numero maximo de inodes do tfs
#define MAX_BOXES 64
#define PIPE_NAME_SIZE 256
#define BOX_NAME_SIZE 32
#define MESSAGE_SIZE 1024

typedef struct {
    int register_pipe;
    size_t max_sessions;
} main_thread_t;

typedef struct {
    char box_name[BOX_NAME_SIZE];
    char pipename[PIPE_NAME_SIZE];
} pub_args_t;

typedef struct {
    char pipename[PIPE_NAME_SIZE];
    int mode;
} manager_args_t;

typedef struct __attribute__((__packed__)){
    char box_name[BOX_NAME_SIZE];
    char publisher[PIPE_NAME_SIZE];

    uint64_t box_size;
    uint64_t n_publishers;
    uint64_t n_subscribers;

    uint8_t last;
} box_t;

// Structs para envio de pedidos por pipes

// Registo de publishers e subscribers
// Ou criação / remoção de caixas
typedef struct __attribute__((__packed__)){
    //the code of the request is sent before
    char client_name_pipe_path[PIPE_NAME_SIZE];
    char box_name[BOX_NAME_SIZE];
} register_request_t;

// Resposta ao pedido de criação/remoção de caixa
typedef struct __attribute__((__packed__)){
    int32_t return_code;
    char error_message[MESSAGE_SIZE];
} box_reply_t;

// Envio de mensagem do publicador para servidor / servidor para subscritor
// ou pedido de listagem de caixas
typedef struct __attribute__((__packed__)){
    char string[MESSAGE_SIZE];
} message_exchange_t;

int create_pipe(char* pipename);
int open_pipe(char* pipename, char mode);
/*void write_pipe(int tx, char const *str);*/
void fill_string(size_t size, char* array);
ssize_t read_pipe(int rx, void* buffer, size_t size);
int compare_structs(const void* a, const void* b);

#endif