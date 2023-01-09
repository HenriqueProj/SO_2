#include "logging.h"
#include <string.h>
#include <utils.h>
#include <unistd.h>

int main(int argc, char **argv) {
    
    char* register_pipe_name = argv[1];    
    char* pipe_name = argv[2];
    char* type = argv[3];
    char* box_name;
    
    // Funciona

    if(argc > 4) {
        box_name = argv[4];
       //fill_string(BOX_NAME_SIZE, box_name);
    }

    int register_pipe = open_pipe(register_pipe_name, 'w');

    if( !create_pipe(pipe_name) || register_pipe == -1)
        return -1;
    
    uint8_t request_code;
    
    // Pedido de criação de caixa
    if(!strcmp(type, "create") ){
        request_code = 3;
        
        register_request_t request;
        strcpy(request.client_name_pipe_path, pipe_name);
        strcpy(request.box_name, box_name);

        printf("Pedido\n");

        if(write(register_pipe, &request_code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
        if(write(register_pipe, &request, sizeof(register_request_t)) < 1)
            exit(EXIT_FAILURE);
        // Envia pedido
    }
    // Pedido de remoção de caixa
    else if(!strcmp(type, "remove") ){
        request_code = 5;
        printf("Pedido\n");
        register_request_t request;
        strcpy(request.client_name_pipe_path, pipe_name);
        strcpy(request.box_name, box_name);

        printf("Remove1");
        if(write(register_pipe, &request_code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
        printf("Remove2");
        if(write(register_pipe, &request, sizeof(register_request_t)) < 1)
            exit(EXIT_FAILURE);
        printf("Remove3");
    }
    // Pedido de listagem de caixas
    else if(!strcmp(type, "list") ){
        printf("Pedido\n");
        request_code = 7;

        // Evita um string overread no read
        // Por causa do \0, assume que o tamanho máximo do ponteiro é o strlen do ponteiro
        char named_pipe[PIPE_NAME_SIZE];
        strcpy(named_pipe, pipe_name);

        //fill_string(PIPE_NAME_SIZE, named_pipe);

        if(write(register_pipe, &request_code, sizeof(uint8_t)) < 1)
            exit(EXIT_FAILURE);
        if(write(register_pipe, &named_pipe, PIPE_NAME_SIZE) < 1)
            exit(EXIT_FAILURE);
        
    }
    
    int tx = open_pipe(pipe_name, 'r');
    if(tx == -1)
        return -1;

    uint8_t code;
    //int32_t return_code;
    //char* error_message;

    read_pipe(tx, &code, sizeof(uint8_t));

    // Criação ou remoção
    if(code == 4 || code == 6){
        box_reply_t box_reply;
        read_pipe(tx, &box_reply, sizeof(box_reply_t) );
        //read_pipe(tx, &error_message, MESSAGE_SIZE*sizeof(char) );

        close(tx);

        if(box_reply.return_code == 1){
            fprintf(stdout, "ERROR %s\n", box_reply.error_message);
            return -1;
        }

        fprintf(stdout, "OK\n");
        // Caixa e struct_box foram criadas / removidas pelo mbroker
    }
    // Listagem
    else if(code == 8){
        
        box_t box;

        //read_pipe(tx, &code, sizeof(uint8_t));
        read_pipe(tx, &box, sizeof(box_t));
        
        if(box.last == 1 && box.box_name[0] == '\0'){
            fprintf(stdout, "NO BOXES FOUND\n");
            return -1;
        }
        printf("LISTING:\n");
        fprintf(stdout, "%s %zu %zu %zu\n", box.box_name, box.box_size, box.n_publishers, box.n_subscribers);

        while(box.last != 1){
            read_pipe(tx, &code, sizeof(uint8_t));

            read_pipe(tx, &box, sizeof(box_t));

            fprintf(stdout, "%s %zu %zu %zu\n", box.box_name, box.box_size, box.n_publishers, box.n_subscribers);
        }
        close(tx);
        
    }   
    else    
        return -1;

    return 0;
}
