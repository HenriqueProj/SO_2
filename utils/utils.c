#include "logging.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <utils.h>

int create_pipe(char* pipename){
    // Remove pipe if it does not exist
    char* src;
    src = malloc(sizeof(char) * (PIPE_NAME_SIZE + 5));
    strcpy(src, "/tmp/");
    strcat(src, pipename);
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

int open_pipe(char* pipename, char mode){
    int tx;
    char* src;
    src = malloc(sizeof(char) * (PIPE_NAME_SIZE + 5));
    strcpy(src, "/tmp/");
    strcat(src, pipename);
    if(mode == 'w') 
        tx = open(src, O_WRONLY);
    else
        tx = open(src, O_RDONLY);
    if (tx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        free(src);
        return 0;
    }
    free(src);
    return tx;
}
/*
void write_pipe(int tx, char const *str) {
    size_t len = strlen(str);
    size_t written = 0;

    while (written < len) {
        ssize_t ret = write(tx, str + written, len - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        written += ret;
    }
}
*/
void fill_string(size_t size, char* array){
    size_t len = strlen(array);
    
    memset(array + len, '\0', size - len);
}

// Lê do pipe e verifica o return value
ssize_t read_pipe(int rx, void* buffer, size_t size){

    ssize_t ret = read(rx, buffer, size);
    
    if (ret == -1) {
        fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
        return -1;
    }
    return ret;
}

int compare_structs(const void* a, const void* b) {
    char const *l = ((box_t *)a)->box_name;
    char const *r = ((box_t *)b)->box_name;

    return strcmp(l , r);
}
