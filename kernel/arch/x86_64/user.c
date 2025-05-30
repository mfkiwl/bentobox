#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/smp.h>
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

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

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

long sys_close(struct registers *r) {
    return fd_close(r->rdi);
}

long sys_execve(struct registers *r) {
    const char *pathname = (const char *)r->rdi;
    char *const *argv = (char *const *)r->rsi;
    char *const *envp = (char *const *)r->rdx;
    
    int argc;
    for (argc = 0; argv[argc]; argc++);

    return exec(pathname, argc, argv, envp);
}

long sys_clone(struct registers *r) {
    return fork(r);
}

#define	R_OK	4
#define	W_OK	2
#define	X_OK	1
#define	F_OK	0

long sys_access(struct registers *r) {
    if (vfs_open(NULL, (const char *)r->rdi)) {
        return F_OK;
    }
    return -1;
}

long sys_wait4(struct registers *r) {
    sched_block(SIGNAL);
    *(uintptr_t *)r->rsi = this->child_exit;
    return 0;
}

long sys_getdents64(struct registers *r) {
    struct linux_dirent64 {
        uint64_t       d_ino;
        int64_t        d_off;
        unsigned short d_reclen;
        unsigned char  d_type;
        char           d_name[];
    };

    int fd_num = r->rdi;
    struct linux_dirent64 *dirp = (struct linux_dirent64 *)r->rsi;
    unsigned int count = r->rdx;

    if (fd_num < 0 || fd_num >= (signed)(sizeof this->fd_table / sizeof(struct fd)) || !this->fd_table[fd_num].node) {
        return -EBADF;
    }

    struct fd *fd = &this->fd_table[fd_num];
    struct vfs_node *dir = fd->node;

    if (dir->type != VFS_DIRECTORY) {
        return -ENOTDIR;
    }
    if (!dirp || count == 0) {
        return -EINVAL;
    }

    struct vfs_node *child = dir->children;
    int entries_to_skip = fd->offset;
    
    while (child && entries_to_skip > 0) {
        child = child->next;
        entries_to_skip--;
    }
    
    int offset = 0;
    struct linux_dirent64 *current_entry = dirp;
    
    while (child) {
        const char *name = child->name;
        int name_len = strlen(name);
        int reclen = ALIGN_UP(sizeof(struct linux_dirent64) + name_len + 1, 8);
        
        if ((unsigned)(offset + reclen) > count)
            break;
        
        current_entry->d_ino = child->inode;
        current_entry->d_off = fd->offset + 1;
        current_entry->d_reclen = reclen;
        switch (child->type) {
            case VFS_DIRECTORY:
                current_entry->d_type = 4; // DT_DIR
                break;
            case VFS_FILE:
                current_entry->d_type = 8; // DT_REG
                break;
            case VFS_CHARDEVICE:
                current_entry->d_type = 2; // DT_CHR
                break;
            case VFS_BLOCKDEVICE:
                current_entry->d_type = 6; // DT_BLK
                break;
            default:
                current_entry->d_type = 0; // DT_UNKNOWN
                break;
        }
        strcpy(current_entry->d_name, name);
        
        current_entry = (void*)current_entry + reclen;
        offset += reclen;
        child = child->next;
        fd->offset++;
    }
    
    return offset;
}

static unsigned int perms_to_mode(enum vfs_node_type type, uint16_t perms) {
    unsigned int mode = 0;
    
    switch (type) {
        case VFS_FILE:
            mode |= S_IFREG;
            break;
        case VFS_DIRECTORY:
            mode |= S_IFDIR;
            break;
        case VFS_CHARDEVICE:
            mode |= S_IFCHR;
            break;
        case VFS_BLOCKDEVICE:
            mode |= S_IFBLK;
            break;
        default:
            mode |= S_IFREG;
            break;
    }
    
    mode |= (perms & 07777);
    return mode;
}

long sys_stat(struct registers *r) {
    const char *pathname = (const char *)r->rdi;
    struct stat *statbuf = (struct stat *)r->rsi;
    
    if (!pathname || !statbuf) {
        return -EFAULT;
    }
    
    struct vfs_node *node = vfs_open(NULL, pathname);
    if (!node) {
        return -ENOENT;
    }
    
    memset(statbuf, 0, sizeof(struct stat));
    
    statbuf->st_mode = perms_to_mode(node->type, node->perms);
    statbuf->st_nlink = 0;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    
    if (node->type == VFS_FILE) {
        statbuf->st_size = node->size;
    } else if (node->type == VFS_DIRECTORY) {
        statbuf->st_size = 4096;
    } else {
        statbuf->st_size = 0;
    }
    return 0;
}

long (*syscalls[256])(struct registers *) = {
    sys_read,
    sys_write,
    sys_open,
    sys_close,
    sys_stat,
    [5 ... 7] = NULL,
    sys_lseek,
    sys_mmap,
    [10 ... 15] = NULL,
    sys_ioctl,
    [17 ... 20] = NULL,
    sys_access,
    [22 ... 55] = NULL,
    sys_clone,
    [57 ... 58] = NULL,
    sys_execve,
    sys_exit,
    sys_wait4,
    [62 ... 157] = NULL,
    sys_arch_prctl,
    [159 ... 185] = NULL,
    sys_gettid,
    [187 ... 216] = NULL,
    sys_getdents64
};

void syscall_handler(struct registers *r) {
    //sched_lock();

    long(*handler)(struct registers *);
    handler = syscalls[r->rax];

    if (!handler) {
        dprintf("%s:%d: unknown syscall %lu\n", __FILE__, __LINE__, r->rax);
        r->rax = -ENOSYS;
        sched_unlock();
        return;
    }

    r->rax = handler(r);
    //sched_unlock();
}

void syscall_bind(uint64_t rax, void *handler) {
    syscalls[rax] = handler;
}