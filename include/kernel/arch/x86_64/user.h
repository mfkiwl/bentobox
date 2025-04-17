#pragma once
#include <kernel/arch/x86_64/idt.h>

void write_cpu_gs(uint64_t value);
void write_kernel_gs(uint64_t value);
uint64_t read_cpu_gs(void);
uint64_t read_kernel_gs(void);
void syscall_handler(struct registers *);
void user_initialize(void);