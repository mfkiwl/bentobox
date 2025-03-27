#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/heap.h>

enum task_state {
    RUNNING,
    PAUSED,
    KILLED,
    MUTEX
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
    uint64_t *stack;
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
void sched_yield(void);
void sched_block(enum task_state reason);
void sched_unblock(struct task *proc);
void sched_sleep(int ms);
void sched_kill(struct task *proc);
struct task *sched_new_task(void *entry, const char *name);