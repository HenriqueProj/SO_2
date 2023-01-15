#include <producer-consumer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// FIXME: Return values??
// FIXME: pcq_head e tail?? Minha implementação - indices de inicio e fim
// (probably errado)
// FIXME: Inicializar cond_vars e mutexes????

// pcq_create: create a queue, with a given (fixed) capacity
//
// Memory: the queue pointer must be previously allocated
// (either on the stack or the heap)
int pcq_create(pc_queue_t *queue, size_t capacity) {
    // Aloca o espaço para as threads
    queue->pcq_buffer = malloc(sizeof(pthread_t) * capacity);

    // Sem espaço
    if (queue->pcq_buffer == NULL) {
        printf("PCQ Malloc: Not enough space\n");
        exit(EXIT_FAILURE);
    }

    queue->pcq_capacity = capacity;
    queue->pcq_current_size = 0;

    queue->pcq_head = 0;
    queue->pcq_tail = 0;

    // Inicializações de mutexes
    if (pthread_mutex_init(&queue->pcq_current_size_lock, NULL) != 0) {
        printf("[ERR]: Mutex init\n");
        return -1;
    }
    if (pthread_mutex_init(&queue->pcq_head_lock, NULL) != 0) {
        printf("[ERR]: Mutex init\n");
        return -1;
    }
    if (pthread_mutex_init(&queue->pcq_tail_lock, NULL) != 0) {
        printf("[ERR]: Mutex init\n");
        return -1;
    }
    if (pthread_mutex_init(&queue->pcq_pusher_condvar_lock, NULL) != 0) {
        printf("[ERR]: Mutex init\n");
        return -1;
    }
    if (pthread_mutex_init(&queue->pcq_popper_condvar_lock, NULL) != 0) {
        printf("[ERR]: Mutex init\n");
        return -1;
    }

    // Inicializações de variáveis de condição
    if (pthread_cond_init(&queue->pcq_pusher_condvar, NULL) != 0) {
        printf("[ERR]: Condvar init\n");
        return -1;
    }
    if (pthread_cond_init(&queue->pcq_popper_condvar, NULL) != 0) {
        printf("[ERR]: Condvar init\n");
        return -1;
    }

    return 0;
}

// pcq_destroy: releases the internal resources of the queue
//
// Memory: does not free the queue pointer itself
int pcq_destroy(pc_queue_t *queue) {
    // Liberta a memória das threads
    for (int i = 0; i < queue->pcq_capacity; i++){
        free(queue->pcq_buffer[i]);
    }
    queue->pcq_capacity = 0;
    queue->pcq_current_size = 0;

    queue->pcq_head = 0;
    queue->pcq_tail = 0;

    // Destruição dos mutexes
    if (pthread_mutex_destroy(&queue->pcq_current_size_lock) != 0) {
        printf("[ERR]: Mutex destroy\n");
        return -1;
    }
    if (pthread_mutex_destroy(&queue->pcq_head_lock) != 0) {
        printf("[ERR]: Mutex destroy\n");
        return -1;
    }
    if (pthread_mutex_destroy(&queue->pcq_tail_lock) != 0) {
        printf("[ERR]: Mutex destroy\n");
        return -1;
    }
    if (pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock) != 0) {
        printf("[ERR]: Mutex destroy\n");
        return -1;
    }
    if (pthread_mutex_destroy(&queue->pcq_popper_condvar_lock) != 0) {
        printf("[ERR]: Mutex destroy\n");
        return -1;
    }

    // Destruição das variáveis de condição
    if (pthread_cond_destroy(&queue->pcq_pusher_condvar) != 0) {
        printf("[ERR]: Condvar destroy\n");
        return -1;
    }
    if (pthread_cond_destroy(&queue->pcq_popper_condvar) != 0) {
        printf("[ERR]: Condvar destroy\n");
        return -1;
    }

    return 0;
}

// pcq_enqueue: insert a new element at the front of the queue
//
// If the queue is full, sleep until the queue has space
int pcq_enqueue(pc_queue_t *queue, void *elem) {
    int should_unlock = 1;

    if (pthread_mutex_lock(&queue->pcq_pusher_condvar_lock) != 0)
        exit(EXIT_FAILURE);

    // Threads todas ocupadas - espera que seja libertado espaço
    while (queue->pcq_current_size == queue->pcq_capacity) {
        pthread_cond_wait(&queue->pcq_pusher_condvar,
                          &queue->pcq_pusher_condvar_lock);
        should_unlock = 0;
    }
    pthread_mutex_lock(&queue->pcq_current_size_lock);


    if (queue->pcq_tail == 0) {
        // Capacidade = 0
        if (queue->pcq_tail == queue->pcq_capacity) {
            queue->pcq_buffer[queue->pcq_tail] = NULL;
        }
        queue->pcq_buffer[queue->pcq_tail] = elem;

        queue->pcq_tail++;
        queue->pcq_current_size++;
    } else {

        queue->pcq_tail++;
        queue->pcq_current_size++;

        // Tail = Capacidade
        if (queue->pcq_tail == queue->pcq_capacity) {
            queue->pcq_buffer[queue->pcq_tail] = NULL;
        }
        queue->pcq_buffer[queue->pcq_tail] = elem;
    }
    pthread_cond_signal(&queue->pcq_popper_condvar);
    pthread_mutex_unlock(&queue->pcq_current_size_lock);
    if(should_unlock == 1) {
        if (pthread_mutex_unlock(&queue->pcq_pusher_condvar_lock) != 0)
            exit(EXIT_FAILURE);
    }
    return 0;
}

// pcq_dequeue: remove an element from the back of the queue
//
// If the queue is empty, sleep until the queue has an element
void *pcq_dequeue(pc_queue_t *queue) {
    int should_unlock = 1;
    if (pthread_mutex_lock(&queue->pcq_popper_condvar_lock) != 0)
        exit(EXIT_FAILURE);

    // Caso não haja threads a correr, espera
    while (queue->pcq_current_size == 0) {
        pthread_cond_wait(&queue->pcq_popper_condvar,
                          &queue->pcq_popper_condvar_lock);
        should_unlock = 0;
    }
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    queue->pcq_current_size--;
    queue->pcq_head++;

    if (queue->pcq_head == queue->pcq_capacity)
        queue->pcq_head = 0;

    pthread_cond_signal(&queue->pcq_pusher_condvar);
    pthread_mutex_unlock(&queue->pcq_current_size_lock);
    if(should_unlock == 1) {
        if (pthread_mutex_unlock(&queue->pcq_popper_condvar_lock) != 0)
            exit(EXIT_FAILURE);
    }
    return NULL;
}