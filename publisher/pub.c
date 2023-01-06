#include "logging.h"
#include <errno.h>
#include <stdbool.h>
#include <pipes.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    /*
    char* register_pipe = argv[0];
    char* box_name = argv[1];
    */
    
    fprintf(stderr, "usage: pub <register_pipe_name> <box_name>\n");
    WARN("unimplemented"); // TODO: implement
    return -1;
}
