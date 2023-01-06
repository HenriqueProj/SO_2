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

int create_pipe(char* pipename){
    // Remove pipe if it does not exist
    if (unlink(pipename) != 0 && errno != ENOENT) {
        printf("Register pipe: Destroy failed");
        return 0;
    }
    // Create pipe
    if (mkfifo(pipename, 0640) != 0) {
        printf("Register pipe: mkfifo failed");
        return 0;
    }
   
    return 1;
}

int open_pipe(char* pipename, char mode){
    int tx;
    if(mode == 'w')
        tx = open(pipename, O_WRONLY);
    else
        tx = open(pipename, O_RDONLY);
    if (tx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    }
    return tx;
}