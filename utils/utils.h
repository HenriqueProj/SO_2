#ifndef __UTILS_UTILS_H__
#define __UTILS_UTILS_H__

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Maximo de boxes, equivalente ao numero maximo de inodes do tfs
#define MAX_BOXES 64

// Tamanhos máximos definidos no enunciado
#define PIPE_NAME_SIZE 256
#define BOX_NAME_SIZE 32
#define MESSAGE_SIZE 1024

// Estrutura correspondente a uma caixa
typedef struct __attribute__((__packed__)) {
    //Nome da caixa
    char box_name[BOX_NAME_SIZE];
    // Nome do publisher associado
    char publisher[PIPE_NAME_SIZE];

    uint64_t box_size;
    // 1 se tem um publisher, 0 caso contrário
    uint64_t n_publishers;
    uint64_t n_subscribers;

    // Array dos subscribers
    char subscribers[MAX_BOXES][PIPE_NAME_SIZE];
    int subscriber_index;
    
    // Para a listagem das caixas
    uint8_t last;
} box_t;

// Structs para envio de pedidos por pipes

// Registo de publishers e subscribers
// Ou criação / remoção de caixas
typedef struct __attribute__((__packed__)) {
    // the code of the request is sent before
    char client_name_pipe_path[PIPE_NAME_SIZE];
    char box_name[BOX_NAME_SIZE];
} register_request_t;

// Resposta ao pedido de criação/remoção de caixa
typedef struct __attribute__((__packed__)) {
    int32_t return_code;
    char error_message[MESSAGE_SIZE];
} box_reply_t;

// Struct que serve de argumento para as funções das threads de registo
typedef struct {
    char client_name_pipe_path[PIPE_NAME_SIZE];
    char box_name[BOX_NAME_SIZE];
} thread_args;

/*
    Cria um pipe, verificando se os return values estão todos como esperado.
*/
int create_pipe(char *pipename);

/*
    Abre um pipe para leitura ou escrita, consoante o caracter mode ('r' ou 'w'),
    verificando se os return values estão todos como esperado.
*/
int open_pipe(char *pipename, char mode);

/*
    Preenche o resto de um array de caracteres com '\0', a fim de
    cumprir o protocolo de envio de mensagens
*/
void fill_string(size_t size, char *array);

/*
    Lê de um pipe, verificando se os return values estão todos como esperado.
*/
ssize_t read_pipe(int rx, void *buffer, size_t size);

/*
    Função de comparação usada na aplicação do qsort,
    para listagem das caixas em ordem alfabética
*/
int compare_structs(const void *a, const void *b);

#endif