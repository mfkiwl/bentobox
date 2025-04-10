#pragma once
#include <stddef.h>

struct fifo {
    int *data;
    int head;
    int tail;
    int count;
    int size;
};

void fifo_init(struct fifo *fifo, int size);
int fifo_is_full(struct fifo *fifo);
int fifo_is_empty(struct fifo *fifo);
int fifo_enqueue(struct fifo *fifo, int value);
int fifo_dequeue(struct fifo *fifo, int *value);