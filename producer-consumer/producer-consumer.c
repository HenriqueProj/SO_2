#include <producer-consumer.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
// FIXME: Return values??
// FIXME: pcq_head e tail?? Minha implementação - indices de inicio e fim (probably errado)
// FIXME: Inicializar cond_vars e mutexes????

// pcq_create: create a queue, with a given (fixed) capacity
//
// Memory: the queue pointer must be previously allocated
// (either on the stack or the heap)
int pcq_create(pc_queue_t *queue, size_t capacity){
    queue->pcq_buffer = malloc( sizeof(pthread_t) * capacity );

    if(queue->pcq_buffer == NULL){
        printf("PCQ Malloc: Not enough space\n");
        exit(EXIT_FAILURE);
    }

    queue->pcq_capacity = capacity;
    queue->pcq_current_size = 0;

    // Out of bounds -> mesmo que NULL (Não existem threads)
    queue->pcq_head = capacity;
    queue->pcq_tail = capacity;

    return 1;
}

// pcq_destroy: releases the internal resources of the queue
//
// Memory: does not free the queue pointer itself
int pcq_destroy(pc_queue_t *queue){
    for(int i = 0; i < queue->pcq_capacity; i++)
        free(queue->pcq_buffer[i]);
    
    queue->pcq_capacity = 0;
    queue->pcq_current_size = 0;

    queue->pcq_head = 0;
    queue->pcq_tail = 0;
    return 1;
}

// pcq_enqueue: insert a new element at the front of the queue
//
// If the queue is full, sleep until the queue has space
int pcq_enqueue(pc_queue_t *queue, void *elem){
    if(queue->pcq_current_size == queue->pcq_capacity){
        // Espera até receber um signal
        // TODO: Maybe SIGALARM para reativar a função??
        pause();
    }

    if(queue->pcq_current_size == 0){
        // Pcq inicialmente vazia - acrescenta a head e tail no mesmo indice
        queue->pcq_head = 0;

        queue->pcq_tail = 0;
    }
    else{
        queue->pcq_tail++;
    }

    queue->pcq_buffer[queue->pcq_current_size] = elem;

    queue->pcq_current_size++;

    return 1;
}

// pcq_dequeue: remove an element from the back of the queue
//
// If the queue is empty, sleep until the queue has an element
void *pcq_dequeue(pc_queue_t *queue){
    if(queue->pcq_current_size == 0){
        // Espera até receber um signal
        // TODO: Maybe SIGALARM para reativar a função??
        pause();
    }
    if(queue->pcq_current_size == 1){
        // 1 thread ativa - esvazia pcq
        queue->pcq_current_size = 0;
        queue->pcq_buffer[0] = NULL;

        // Out of bounds
        queue->pcq_head = queue->pcq_capacity;
        queue->pcq_tail = queue->pcq_capacity;
    }
    else{
        // Puxa as threads ativas 1 indice para trás, coloca a última a NULL e dá tail--
        for(int i = 1; i < queue->pcq_current_size; i++){
            queue->pcq_buffer[i - 1] = queue->pcq_buffer[i];
        }
        queue->pcq_buffer[queue->pcq_current_size - 1] = NULL;
    
        queue->pcq_tail--;
        queue->pcq_current_size--;
    }

    return NULL;
}