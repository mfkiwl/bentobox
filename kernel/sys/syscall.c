#include "kernel/arch/x86_64/user.h"
#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/fd.h>
#include <kernel/vfs.h>
#include <kernel/mmu.h>
#include <kernel/posix.h>
#include <kernel/sched.h>
#include <kernel/assert.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/syscall.h>

long sys_exit(struct registers *r) {
    //dprintf("%s:%d: %s: exiting with status %lu\n", __FILE__, __LINE__, __func__, r->rdi);
    sched_kill(this, r->rdi);
    __builtin_unreachable();
}

long sys_read(struct registers *r) {
    struct fd *fd = &this->fd_table[r->rdi];
    if (!fd->node) {
        return -1;
    }
    if (fd->node->read) {
        long ret = fd->node->read(fd->node, (void *)r->rsi, fd->offset, r->rdx);
        fd->offset += ret;
        return ret;
    }
    return 0;
}

long sys_write(struct registers *r) {
    struct fd *fd = &this->fd_table[r->rdi];
    if (!fd->node) {
        return -1;
    }
    if (fd->node->write) {
        long ret = fd->node->write(fd->node, (void *)r->rsi, fd->offset, r->rdx);
        fd->offset += ret;
        return ret;
    }
    return 0;
}

long sys_getpid(struct registers *r) {
    return this->pid;
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

long sys_wait4(struct registers *r) {
    if (!this->children) {
        return -ECHILD;
    }
    sched_block(PAUSED);
    *(uintptr_t *)r->rsi = this->child_exit;
    return 0;
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
        case 0x5413:
            dprintf("%s:%d: TODO: implement TIOCGWINSZ (get window size)\n", __FILE__, __LINE__);
            return -EINVAL;
        default:
            dprintf("%s:%d: %s: function 0x%lx not implemented\n", __FILE__, __LINE__, __func__, op);
            return -EINVAL;
    }
}

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

long sys_access(struct registers *r) {
    if (vfs_open(NULL, (const char *)r->rdi)) {
        return F_OK;
    }
    return -1;
}

long sys_dup(struct registers *r) {
    unimplemented;
    return -ENOSYS;
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
                current_entry->d_type = DT_DIR;
                break;
            case VFS_FILE:
                current_entry->d_type = DT_REG;
                break;
            case VFS_CHARDEVICE:
                current_entry->d_type = DT_CHR;
                break;
            case VFS_BLOCKDEVICE:
                current_entry->d_type = DT_BLK;
                break;
            default:
                current_entry->d_type = DT_UNKNOWN;
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

static unsigned int convert_mode(enum vfs_node_type type, uint16_t perms) {
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
    
    statbuf->st_mode = convert_mode(node->type, node->perms);
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

long sys_fstat(struct registers *r) {
    struct fd *fd = &this->fd_table[r->rdi];
    struct stat *statbuf = (struct stat *)r->rsi;
    
    if (!fd->node || !statbuf) {
        return -EFAULT;
    }
    
    struct vfs_node *node = fd->node;
    
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

long sys_arch_prctl(struct registers *r) {
    switch (r->rdi) {
        case 0x1002: /* ARCH_SET_FS */
            write_fs(r->rsi);
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
    //int clockid = r->rdi;
    struct timespec *user_ts = (struct timespec *)r->rsi;
    
    if (!user_ts)
        return -EINVAL;

    struct timespec ts;
    hpet_read_time(&ts.tv_sec, &ts.tv_nsec);
    memcpy(user_ts, &ts, sizeof ts);
    return 0;
}

long (*syscalls[456])(struct registers *) = {
    [SYS_read]          = sys_read,
    [SYS_write]         = sys_write,
    [SYS_open]          = sys_open,
    [SYS_close]         = sys_close,
    [SYS_stat]          = sys_stat,
    [SYS_fstat]         = sys_fstat,
    [SYS_lseek]         = sys_lseek,
    [SYS_mmap]          = sys_mmap,
    [13]                = sys_rt_sigaction,
    [14]                = sys_rt_sigprocmask,
    [SYS_ioctl]         = sys_ioctl,
    [SYS_access]        = sys_access,
    [32]                = sys_dup,
    [39]                = sys_getpid,
    [SYS_clone]         = sys_clone,
    [SYS_execve]        = sys_execve,
    [SYS_exit]          = sys_exit,
    [SYS_wait4]         = sys_wait4,
    [102]               = sys_getuid,
    [104]               = sys_getgid,
    [107]               = sys_geteuid,
    [108]               = sys_getegid,
    [110]               = sys_getppid,
    [121]               = sys_getgpid,
    [SYS_arch_prctl]    = sys_arch_prctl,
    [SYS_gettid]        = sys_getpid,
    [SYS_getdents64]    = sys_getdents64,
    [SYS_clock_gettime] = sys_clock_gettime
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