#include "kernel/arch/x86_64/vmm.h"
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/user.h>
#include <kernel/mmu.h>
#include <kernel/printf.h>

extern void syscall_entry(void);

uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

void wrmsr(uint32_t msr, uint64_t val) {
    asm volatile ("wrmsr" : : "a"((uint32_t)val), "d"((uint32_t)(val >> 32)), "c"(msr));
}

uint64_t read_kernel_gs(void) {
    return rdmsr(IA32_GS_KERNEL_MSR);
}

void write_kernel_gs(uint64_t value) {
    wrmsr(IA32_GS_KERNEL_MSR, value);
}

void user_initialize(void) {
    wrmsr(IA32_EFER, rdmsr(IA32_EFER) | (1 << 0));
    wrmsr(IA32_STAR, ((uint64_t)0x08 << 32) | ((uint64_t)0x13 << 48));
    wrmsr(IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(IA32_CSTAR, 0);
    wrmsr(IA32_CSTAR + 1, 0x200);
}

extern long sys_read(struct registers *);
extern long sys_write(struct registers *);
extern long sys_exit(struct registers *);

long sys_gettid(struct registers *r) {
    return this_core()->current_proc->pid;
}

long sys_arch_prctl(struct registers *r) {
    switch (r->rdi) {
        case 0x1002: /* ARCH_SET_FS */
            wrmsr(IA32_FS_BASE, r->rsi);
            break;
        default:
            dprintf("%s:%d: %s: function 0x%lx not implemented\n", __FILE__, __LINE__, __func__, r->rdi);
            return -22; /* EINVAL */
    }
    return 0;
}

// [x ... y] = NULL,
long (*syscalls[256])(struct registers *) = {
    sys_read,
    sys_write,
    [2 ... 59] = NULL,
    sys_exit,
    [61 ... 157] = NULL,
    sys_arch_prctl,
    [159 ... 185] = NULL,
    sys_gettid
};

void syscall_handler(struct registers *r) {
    sched_lock();

    long(*handler)(struct registers *);
    handler = syscalls[r->rax];

    if (!handler) {
        dprintf("%s:%d: invalid syscall %lu\n", __FILE__, __LINE__, r->rax);
        return;
    }

    r->rax = handler(r);
    sched_unlock();
}

void syscall_bind(uint64_t rax, void *handler) {
    syscalls[rax] = handler;
}