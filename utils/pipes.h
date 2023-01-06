#ifndef __UTILS_PIPES_H__
#define __UTILS_PIPES_H__

#include <stdio.h>
#include <stdlib.h>

int create_pipe(char* pipename);
int open_pipe(char* pipename, char mode);


#endif