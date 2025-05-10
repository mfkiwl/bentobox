#pragma once
#include <kernel/arch/x86_64/idt.h>

#define IA32_GS_KERNEL_MSR  0xC0000102
#define IA32_FS_BASE        0xC0000100
#define IA32_EFER           0xC0000080
#define IA32_STAR           0xC0000081
#define IA32_LSTAR          0xC0000082
#define IA32_CSTAR          0xC0000083

void write_kernel_gs(uint64_t value);
uint64_t read_kernel_gs(void);
void syscall_handler(struct registers *);
void user_initialize(void);