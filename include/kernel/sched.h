#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/vfs.h>
#include <kernel/malloc.h>

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
    struct vfs_node *fd_table[16];
};

void sched_install(void);
void sched_start_all_cores(void);
void sched_yield(void);
void sched_block(enum task_state reason);
void sched_unblock(struct task *proc);
void sched_sleep(int ms);
void sched_kill(struct task *proc);
void sched_idle(void);
struct task *sched_new_task(void *entry, const char *name, int cpu);