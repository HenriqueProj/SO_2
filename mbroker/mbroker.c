#include "logging.h"

bool register_publisher(char* client_named_pipe_path, char* box_name){
    char[1] buffer;

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    int box_handle = tfs_lookup(name, root_dir_inode);
    
    // Box não existe
    if( box_handle == -1)
        return false;
    
    // Box não vazia - há um publisher na box já (ou a box não se apagou depois de dar kill no publisher)
    if(tfs_read(box_handle, buffer, 1) != 0)
        return false;
    
    // Falha a criar ou abrir o pipe publisher -> servidor
    if (mkfifo(client_named_pipe_path, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        return false;
    }
    int tx = open(client_named_pipe_path, O_RDONLY);
    if (tx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return false;
    }

    // Success!!
    return true;
}

bool register_sub(char* client_named_pipe_path, char* box_name){
}
bool create_box(char* client_named_pipe_path, char* box_name){
}
bool remove_box(char* client_named_pipe_path, char* box_name){
}
bool list_boxes(char* client_named_pipe_path){
}

int main(int argc, char **argv) {
    (void)argc;
    
    char* pipename = argv[0];
    int max_sessions = (int) argv[1];

    fprintf(stderr, "usage: mbroker <pipename>\n");
    WARN("unimplemented"); // TODO: implement
    return -1;
}
