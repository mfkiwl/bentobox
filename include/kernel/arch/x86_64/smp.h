#pragma once
#include <stdint.h>

struct cpu {
    uint64_t id;
};

void smp_initialize(void);
struct cpu *this_core(void);