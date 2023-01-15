#ifndef __UTILS_UTILS_H__
#define __UTILS_UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

// Maximo de boxes, equivalente ao numero maximo de inodes do tfs
#define MAX_BOXES 64
#define PIPE_NAME_SIZE 256
#define BOX_NAME_SIZE 32
#define MESSAGE_SIZE 1024

//tive de tirar o attribute packed por causa da variável de condição
typedef struct __attribute__((__packed__)){
    char box_name[BOX_NAME_SIZE];
    char publisher[PIPE_NAME_SIZE];

    uint64_t box_size;
    uint64_t n_publishers;
    uint64_t n_subscribers;
    char subscribers[MAX_BOXES][PIPE_NAME_SIZE];
    int subscriber_index;
    uint8_t last;
    pthread_cond_t condition;
    pthread_mutex_t mutex;
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

typedef struct {
    register_request_t publisher;
    int box_index;
} pub_args_t;

typedef struct {
    char* pipe_name;
    int n;
} man_args_t;

int create_pipe(char* pipename);
int open_pipe(char* pipename, char mode);
/*void write_pipe(int tx, char const *str);*/
void fill_string(size_t size, char* array);
ssize_t read_pipe(int rx, void* buffer, size_t size);
int compare_structs(const void* a, const void* b);

#endif