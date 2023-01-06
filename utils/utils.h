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

} box_t;



int create_pipe(char* pipename);
int open_pipe(char* pipename, char mode);
/*void write_pipe(int tx, char const *str);*/
void fill_string(int size, char* array);

#endif