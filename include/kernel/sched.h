#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/vfs.h>
#include <kernel/elf64.h>
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

struct task_elf {
    Elf64_Sym *symtab;
    const char *strtab;
    int symbol_count;
};

struct task {
    uint64_t stack;
    uint64_t kernel_stack;
    uint64_t gs;
    struct registers ctx;
    struct task *next;
    struct task *prev;
    const char *name;
    uint64_t *pml4;
    long pid;
    enum task_state state;
    struct heap *heap;
    struct task_time time;
    struct vfs_node *fd_table[16];
    struct task_elf elf;
};

void sched_install(void);
void sched_start_all_cores(void);
void sched_yield(void);
void sched_start_timer(void);
void sched_stop_timer(void);
void sched_lock(void);
void sched_unlock(void);
void sched_block(enum task_state reason);
void sched_unblock(struct task *proc);
void sched_sleep(int ms);
void sched_kill(struct task *proc, int status);
void sched_idle(void);
struct task *sched_new_task(void *entry, const char *name, int cpu);
struct task *sched_new_user_task(void *entry, const char *name, int cpu);