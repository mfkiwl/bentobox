#pragma once
#include <stdint.h>
#include <stdatomic.h>

struct cpu {
    uint64_t id;
    uint64_t lapic_id;
    // TODO: store current pm here

    struct task *processes;
    struct task *current_proc;
    atomic_flag sched_lock;
};

void smp_initialize(void);
struct cpu *get_core(int core);
struct cpu *this_core(void);