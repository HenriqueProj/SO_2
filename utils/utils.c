#include "logging.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utils.h>

// Tamanho do prefixo /tmp/ a ser adicionado aos nomes dos pipes
#define TMP_SIZE 5

int create_pipe(char *pipename) {
    //"/tmp/" adicionado ao nome do pipe, para que a criação de pipes 
    // funcione na plataforma sigma.

    char *src;

    // Aloca o espaço para o pipename + /tmp/
    src = malloc(sizeof(char) * (PIPE_NAME_SIZE + TMP_SIZE));

    // Adiciona o prefixo ao nome do pipe
    strcpy(src, "/tmp/");
    strcat(src, pipename);

    // Falha ao destruir o pipe, caso já exista
    if (unlink(src) != 0 && errno != ENOENT) {
        printf("Register pipe: Destroy failed");
        free(src);
        return 0;
    }
    // Create pipe
    if (mkfifo(src, 0640) != 0) {
        printf("Register pipe: mkfifo failed");
        free(src);
        return 0;
    }
    free(src);
    return 1;
}

int open_pipe(char *pipename, char mode) {
    //"/tmp/" adicionado ao nome do pipe, para que a criação de pipes 
    // funcione na plataforma sigma.

    int tx;
    char *src;

    // Aloca o espaço para o pipename + /tmp/
    src = malloc(sizeof(char) * (PIPE_NAME_SIZE + TMP_SIZE));

    // Adiciona o prefixo ao nome do pipe
    strcpy(src, "/tmp/");
    strcat(src, pipename);

    // Modo de escrita
    if (mode == 'w')
        tx = open(src, O_WRONLY);
    
    // Modo de leitura
    else
        tx = open(src, O_RDONLY);
    
    // Erro na abertura do pipe
    if (tx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        free(src);
        return 0;
    }
    free(src);
    return tx;
}

void fill_string(size_t size, char *array) {
    size_t len = strlen(array);

    memset(array + len, '\0', size - len);
}

// Lê do pipe e verifica o return value
ssize_t read_pipe(int rx, void *buffer, size_t size) {

    ssize_t ret = read(rx, buffer, size);

    // Falha na leitura do pipe
    if (ret == -1) {
        fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
        return -1;
    }
    return ret;
}

int compare_structs(const void *a, const void *b) {
    // Converte void* em box_t* (ponteiro para struct de caixa)
    // e retira os nomes das caixas
    char const *box_name1 = ((box_t *)a)->box_name;
    char const *box_name2 = ((box_t *)b)->box_name;

    // Função de comparação escolhida - strcmp
    return strcmp(box_name1, box_name2);
}
