#include "kernel/vfs.h"
#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/user.h>
#include <kernel/mmu.h>
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

void user_initialize(void) {
    wrmsr(IA32_EFER, rdmsr(IA32_EFER) | (1 << 0));
    wrmsr(IA32_STAR, ((uint64_t)0x08 << 32) | ((uint64_t)0x13 << 48));
    wrmsr(IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(IA32_CSTAR, 0);
    wrmsr(IA32_CSTAR + 1, 0x200);
}

long sys_gettid(struct registers *r) {
    return this->pid;
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

long sys_ioctl(struct registers *r) {
    int fd = r->rdi;
    int op = r->rsi;

    switch (op) {
        case 0x5401: /* TCGETS */
            if (fd < 3) {
                return 0;
            } else {
                return -ENOTTY;
            }
            break;
        default:
            dprintf("%s:%d: %s: function 0x%lx not implemented\n", __FILE__, __LINE__, __func__, r->rdi);
            return -EINVAL;
    }
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

long sys_lseek(struct registers *r) {
    struct fd *fd = &this->fd_table[r->rdi];
    off_t offset = r->rsi;
    int whence = r->rdx;

    if (fd->node->type == VFS_CHARDEVICE) {
        return -ESPIPE;
    }

    switch (whence) {
        case SEEK_SET:
            fd->offset = offset;
            break;
        case SEEK_CUR:
            fd->offset += offset;
            break;
        case SEEK_END:
            fd->offset = fd->node->size + offset;
            break;
    }

    return fd->offset;
}

long sys_open(struct registers *r) {
    const char *pathname = (const char *)r->rdi;
    int flags = r->rsi;
    mode_t mode = r->rdx;
    (void)mode;

    return fd_open(pathname, flags);
}

long sys_execve(struct registers *r) {
    const char *pathname = (const char *)r->rdi;
    char *const *argv = (char *const *)r->rsi;
    char *const *envp = (char *const *)r->rdx;
    
    int argc;
    for (argc = 0; argv[argc]; argc++);

    exec(pathname, argc, argv, envp);
    return 0;
}

// [x ... y] = NULL,
long (*syscalls[256])(struct registers *) = {
    sys_read,
    sys_write,
    sys_open,
    [3 ... 7] = NULL,
    sys_lseek,
    sys_mmap,
    [10 ... 15] = NULL,
    sys_ioctl,
    [17 ... 58] = NULL,
    sys_execve,
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
        dprintf("%s:%d: unknown syscall %lu\n", __FILE__, __LINE__, r->rax);
        r->rax = -ENOSYS;
        return;
    }

    r->rax = handler(r);
    sched_unlock();
}

void syscall_bind(uint64_t rax, void *handler) {
    syscalls[rax] = handler;
}