#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/heap.h>

struct task {
    struct registers ctx;
    struct task *next;
    struct task *prev;
    const char *name;
    uint16_t *stack;
    uint64_t *page_dir;
    uint8_t ring;
    long pid;
    bool killed;
    struct heap *heap;
};

extern struct task *processes;
extern struct task *current_proc;

void sched_install(void);
void sched_start(void);