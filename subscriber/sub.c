#include "logging.h"

int main(int argc, char **argv) {
    (void)argc;
    
    char* register_pipe = argv[0];
    char* box_name = argv[1];
    
    fprintf(stderr, "usage: sub <register_pipe_name> <box_name>\n");
    WARN("unimplemented"); // TODO: implement
    return -1;
}
