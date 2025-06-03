#pragma once
#include <stdint.h>
#include <stdatomic.h>

#define SMP_MAX_CORES 32

struct cpu {
    uint64_t id;
    uint64_t lapic_id;
    uintptr_t *pml4;

    struct task *processes;
    struct task *current_proc;
    struct task *cleaner_proc;
    struct task *idle_proc;
    struct task *terminated_processes;
    atomic_flag sched_lock;
    atomic_flag vmm_lock;
};

void smp_initialize(void);
struct cpu *get_core(int core);
struct cpu *this_core(void);