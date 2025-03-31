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
    atomic_flag sched_lock;
};

void smp_initialize(void);
struct cpu *get_core(int core);
struct cpu *this_core(void);