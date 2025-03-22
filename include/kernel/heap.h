#pragma once
#include <stdint.h>
#include <stdbool.h>

#define HEAP_MAGIC 0x58524332

struct heap_block {
    struct heap_block *next;
    struct heap_block *prev;
    uint32_t size; 
    uint32_t magic;
};

struct heap {
    struct heap_block *head;
};

extern struct heap *kernel_heap;

void *kmalloc(size_t n);
void  kfree(void *ptr);

struct heap *heap_create();
void *heap_alloc(struct heap *h, uint64_t n);
void  heap_free(void *ptr);