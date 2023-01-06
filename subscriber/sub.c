#include "logging.h"
#include <utils.h>

int main(int argc, char **argv) {
    (void)argc;
    char* register_pipe = argv[0];
    char* sub_pipename = argv[1];
    char* box_name = argv[2];

    fill_string(PIPE_NAME_SIZE, sub_pipename);
    fill_string(BOX_NAME_SIZE, box_name);

    if( !create_pipe(sub_pipename) || !open_pipe(register_pipe, 'w'))
        return -1;

    fprintf(stderr, "usage: sub <register_pipe_name> <box_name>\n");
    WARN("unimplemented"); // TODO: implement
    return -1;
}
