#include "logging.h"

static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> list\n");
}

int main(int argc, char **argv) {
    (void)argc;

    char* register_pipe = argv[0];    
    char* type = argv[1];
    char* box_name;

    if(argv > 2)
        box_name = argv[2];

    print_usage();
    WARN("unimplemented"); // TODO: implement
    return -1;
}
