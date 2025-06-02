#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/arch/x86_64/user.h>
#include <kernel/fd.h>
#include <kernel/vfs.h>
#include <kernel/mmu.h>
#include <kernel/sched.h>
#include <kernel/assert.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/syscall.h>

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

uint64_t read_gs(void) {
    return rdmsr(IA32_GS_BASE);
}

void write_gs(uint64_t value) {
    wrmsr(IA32_GS_BASE, value);
}

void user_initialize(void) {
    wrmsr(IA32_EFER, rdmsr(IA32_EFER) | (1 << 0));
    wrmsr(IA32_STAR, ((uint64_t)0x08 << 32) | ((uint64_t)0x13 << 48));
    wrmsr(IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(IA32_CSTAR, 0);
    wrmsr(IA32_CSTAR + 1, 0x200);
}

long sys_arch_prctl(struct registers *r) {
    switch (r->rdi) {
        case 0x1002: /* ARCH_SET_FS */
            wrmsr(IA32_FS_BASE, r->rsi);
            this->fs = r->rsi;
            break;
        default:
            dprintf("%s:%d: %s: function 0x%lx not implemented\n", __FILE__, __LINE__, __func__, r->rdi);
            return -EINVAL;
    }
    return 0;
}

long sys_mmap(struct registers *r) {
    void *addr = (void *)r->rdi;
    size_t length = r->rsi;
    int prot = r->rdx;
    int flags = r->r10;
    int fd = r->r8;
    off_t offset = r->r9;

    uint64_t vma_flags = PTE_USER;
    if (prot & PROT_READ) vma_flags |= PTE_PRESENT;
    if (prot & PROT_WRITE) vma_flags |= PTE_WRITABLE;

    size_t pages = ALIGN_UP(length, PAGE_SIZE) / PAGE_SIZE;

    if (addr == NULL) {
        addr = vma_map(this->vma, pages, 0, 0, vma_flags);
    }

    if (flags & MAP_ANONYMOUS) {
        if (offset != 0 || fd != -1) {
            return -EINVAL;
        }
        memset(addr, 0, length);
    }

    return (long)addr;
}

long sys_rt_sigaction(struct registers *r) {
    return 0;
}

long sys_rt_sigprocmask(struct registers *r) {
    return 0;
}

long sys_getuid(struct registers *r) {
    return 0;
}

long sys_getgid(struct registers *r) {
    return 0;
}

long sys_geteuid(struct registers *r) {
    return 0;
}

long sys_getegid(struct registers *r) {
    return 0;
}

long sys_getppid(struct registers *r) {
    if (this->parent)
        return this->parent->pid;
    else
        return -1;
}

long sys_getgpid(struct registers *r) {
    return r->rdi;
}

long sys_clock_gettime(struct registers *r) {
    int clockid = r->rdi;
    struct timespec *user_ts = (struct timespec *)r->rsi;
    
    if (!user_ts)
        return -EINVAL;

    struct timespec ts;
    hpet_read_time(&ts.tv_sec, &ts.tv_nsec);
    memcpy(user_ts, &ts, sizeof ts);
    return 0;
}

long (*syscalls[456])(struct registers *) = {
    sys_read,
    sys_write,
    sys_open,
    sys_close,
    sys_stat,
    sys_fstat,
    [6 ... 7] = NULL,
    sys_lseek,
    sys_mmap,
    [10 ... 12] = NULL,
    sys_rt_sigaction,
    sys_rt_sigprocmask,
    NULL,
    sys_ioctl,
    [17 ... 20] = NULL,
    sys_access,
    [22 ... 31] = NULL,
    sys_dup,
    [33 ... 38] = NULL,
    sys_getpid,
    [40 ... 55] = NULL,
    sys_clone,
    [57 ... 58] = NULL,
    sys_execve,
    sys_exit,
    sys_wait4,
    [62 ... 101] = NULL,
    sys_getuid,
    NULL,
    sys_getgid,
    [105 ... 106] = NULL,
    sys_geteuid,
    sys_getegid,
    NULL,
    sys_getppid,
    [111 ... 120] = NULL,
    sys_getgpid,
    [122 ... 157] = NULL,
    sys_arch_prctl,
    [159 ... 185] = NULL,
    sys_getpid,
    [187 ... 216] = NULL,
    sys_getdents64,
    [218 ... 227] = NULL,
    sys_clock_gettime
};

void syscall_handler(struct registers *r) {
    long(*handler)(struct registers *);
    handler = syscalls[r->rax];

    if (!handler) {
        dprintf("%s:%d: unknown syscall %lu\n", __FILE__, __LINE__, r->rax);
        r->rax = -ENOSYS;
        sched_unlock();
        return;
    }

    r->rax = handler(r);
}

void syscall_bind(uint64_t rax, void *handler) {
    syscalls[rax] = handler;
}