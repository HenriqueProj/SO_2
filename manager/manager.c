#include "logging.h"
#include <string.h>
#include <utils.h>
#include <unistd.h>

int main(int argc, char **argv) {

    char* register_pipe = argv[0];    
    char* pipe_name = argv[1];
    char* type = argv[2];
    char* box_name;
    
    fill_string(PIPE_NAME_SIZE, pipe_name);
    
    if(argc > 3){
        box_name = argv[3];
        fill_string(PIPE_NAME_SIZE, box_name);
    }

    if( !create_pipe(pipe_name) || !open_pipe(register_pipe, 'w'))
        return -1;

    // Pedido de criação de caixa
    if(!strcmp(type, "create") ){
        register_request_t request = {3, pipe_name, box_name};

        // Envia pedido
    }
    // Pedido de remoção de caixa
    else if(!strcmp(type, "remove") ){
        register_request_t request = {5, pipe_name, box_name};

        // Envia pedido
    }
    // Pedido de listagem de caixas
    else{
        message_exchange_t request = {7, pipe_name};

        // Envia pedido
    }
    
    int tx = open_pipe(pipe_name, 'r');
    if(tx == -1)
        return -1;

    uint8_t code;
    int32_t return_code;
    char* error_message;

    read_pipe(tx, &code, sizeof(uint8_t));

    // Criação ou remoção
    if(code == 4 || code == 6){
        read_pipe(tx, &return_code, sizeof(int32_t) );
        read_pipe(tx, &error_message, MESSAGE_SIZE*sizeof(char) );

        if(return_code == 1){
            fprintf(stdout, "ERROR %s\n", error_message);
            return -1;
        }

        fprintf(stdout, "OK\n");
        // Caixa e struct_box foram criadas / removidas pelo mbroker
    }
    // Listagem
    else if(code == 8){
        /*
        uint8_t last;
        char* box_name;
        
        read_pipe(tx, &code, sizeof(uint8_t));
        read_pipe(tx, &box_name, BOX_NAME_SIZE*sizeof(char) );
        
        if(last == 1 && box_name[0] == '\0'){
            fprintf(stdout, "NO BOXES FOUND\n");
            return -1;
        }

        uint64_t box_size;
        uint64_t n_publishers;
        uint64_t n_subscribers;
        
        read_pipe(tx, &box_size, sizeof(uint64_t));
        read_pipe(tx, &n_publishers, sizeof(uint64_t));
        read_pipe(tx, &n_subscribers, sizeof(uint64_t));

        fprintf(stdout, "%s %zu %zu %zu\n", box_name, box_size, n_publishers, n_subscribers);

        while(last != -1){
            read_pipe(tx, &code, sizeof(uint8_t));
            read_pipe(tx, &box_name, BOX_NAME_SIZE*sizeof(char) );
            read_pipe(tx, &box_size, sizeof(uint64_t));
            read_pipe(tx, &n_publishers, sizeof(uint64_t));
            read_pipe(tx, &n_subscribers, sizeof(uint64_t));

            fprintf(stdout, "%s %zu %zu %zu\n", box_name, box_size, n_publishers, n_subscribers);
        }
        */
    }   
    else    
        return -1;

    return 0;
}
