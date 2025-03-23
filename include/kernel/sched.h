#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/heap.h>

enum task_state {
    RUNNING,
    PAUSED,
    KILLED
};

struct task_time {
    uint64_t start;
    uint64_t end;
    uint64_t last;
};

struct task {
    struct registers ctx;
    struct task *next;
    struct task *prev;
    const char *name;
    uint16_t *stack;
    uint64_t *page_dir;
    uint8_t ring;
    long pid;
    enum task_state state;
    struct heap *heap;
    struct task_time time;
};

extern struct task *processes;
extern struct task *current_proc;

void sched_install(void);
void sched_start(void);