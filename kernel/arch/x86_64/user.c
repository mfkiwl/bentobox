#include <kernel/arch/x86_64/idt.h>
#include <kernel/printf.h>

#define IA32_GS_MSR         0xC0000101
#define IA32_GS_KERNEL_MSR  0xC0000102
#define IA32_EFER           0xC0000080
#define IA32_STAR           0xC0000081
#define IA32_LSTAR          0xC0000082
#define IA32_CSTAR          0xC0000083

uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

void wrmsr(uint32_t msr, uint64_t val) {
    asm volatile ("wrmsr" : : "a"((uint32_t)val), "d"((uint32_t)(val >> 32)), "c"(msr));
}

uint64_t read_cpu_gs(void) {
    return rdmsr(IA32_GS_MSR);
}

void write_cpu_gs(uint64_t value) {
    wrmsr(IA32_GS_MSR, value);
}

uint64_t read_kernel_gs(void) {
    return rdmsr(IA32_GS_KERNEL_MSR);
}

void write_kernel_gs(uint64_t value) {
    wrmsr(IA32_GS_KERNEL_MSR, value);
}

void syscall_handler(struct registers *r) {
    printf("System call handler invoked!\n");
}

extern void syscall_entry(void);

void user_initialize(void) {
    uint64_t efer = rdmsr(IA32_EFER);
    efer |= (1 << 0);
    wrmsr(IA32_EFER, efer);
    uint64_t star = 0;
    star |= ((uint64_t)0x08 << 32);
    star |= ((uint64_t)0x0b << 48);
    wrmsr(IA32_STAR, star);
    wrmsr(IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(IA32_CSTAR, 0);
    wrmsr(IA32_CSTAR + 1, 0x200);
}