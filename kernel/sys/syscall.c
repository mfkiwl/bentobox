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

long sys_exit(long status) {
    //dprintf("%s:%d: %s: exiting with status %lu\n", __FILE__, __LINE__, __func__, r->rdi);
    sched_kill(this, status);
    __builtin_unreachable();
}

long sys_read(int fd_num, void *buffer, size_t len) {
    struct fd *fd = &this->fd_table[fd_num];
    if (!fd->node) {
        return -1;
    }
    if (fd->node->read) {
        long ret = fd->node->read(fd->node, buffer, fd->offset, len);
        fd->offset += ret;
        return ret;
    }
    return 0;
}

long sys_write(int fd_num, void *buffer, size_t len) {
    struct fd *fd = &this->fd_table[fd_num];
    if (!fd->node) {
        return -1;
    }
    if (fd->node->write) {
        long ret = fd->node->write(fd->node, buffer, fd->offset, len);
        fd->offset += ret;
        return ret;
    }
    return 0;
}

long sys_getpid(void) {
    return this->pid;
}

long sys_execve(const char *pathname, char *const *argv, char *const *envp) {
    int argc;
    for (argc = 0; argv[argc]; argc++);

    return exec(pathname, argc, argv, envp);
}

long sys_clone(struct registers *r) {
    return fork(r);
}

long sys_wait4(int pid, int *wstatus) {
    if (!this->children) {
        return -ECHILD;
    }
    sched_block(PAUSED);
    *wstatus = this->child_exit;
    return 0;
}

long sys_ioctl(int fd, int op) {
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

long sys_lseek(int fd_num, off_t offset, int whence) {
    struct fd *fd = &this->fd_table[fd_num];

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

long sys_open(const char *pathname, int flags, mode_t mode) {
    (void)mode;
    return fd_open(pathname, flags);
}

long sys_close(int fd) {
    return fd_close(fd);
}

long sys_access(const char *pathname) {
    if (vfs_open(NULL, pathname)) {
        return F_OK;
    }
    return -1;
}

long sys_dup() {
    unimplemented;
    return -ENOSYS;
}

struct linux_dirent64 {
    uint64_t       d_ino;
    int64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

long sys_getdents64(int fd_num, struct linux_dirent64 *dirp, unsigned int count) {
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

long sys_stat(const char *pathname, struct stat *statbuf) {
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

long sys_fstat(int fd_num, struct stat *statbuf) {
    struct fd *fd = &this->fd_table[fd_num];

    if (!fd->node || !statbuf) {
        return -EFAULT;
    }
    
    struct vfs_node *node = fd->node;
    
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

long sys_arch_prctl(int op, long extra) {
    switch (op) {
        case 0x1002: /* ARCH_SET_FS */
            write_fs(extra);
            this->fs = extra;
            break;
        default:
            dprintf("%s:%d: %s: function 0x%lx not implemented\n", __FILE__, __LINE__, __func__, op);
            return -EINVAL;
    }
    return 0;
}

long sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
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

long sys_munmap() {
    return -ENOSYS;
}

long sys_rt_sigaction() {
    return 0;
}

long sys_rt_sigprocmask() {
    return 0;
}

long sys_getuid(void) {
    return 0;
}

long sys_getgid(void) {
    return 0;
}

long sys_geteuid(void) {
    return 0;
}

long sys_getegid(void) {
    return 0;
}

long sys_getppid(void) {
    if (this->parent)
        return this->parent->pid;
    else
        return -1;
}

long sys_getpgid(int pid) {
    if (!pid)
        return this->pid;
    return pid;
}

long sys_clock_gettime(int clockid, struct timespec *user_ts) {
    (void)clockid;
    if (!user_ts)
        return -EINVAL;

    struct timespec ts;
    hpet_read_time(&ts.tv_sec, &ts.tv_nsec);
    memcpy(user_ts, &ts, sizeof ts);
    return 0;
}

typedef long (*syscall_func)(long, long, long, long, long, long);

static syscall_func syscalls[] = {
    [SYS_read]          = (syscall_func)(uintptr_t)sys_read,
    [SYS_write]         = (syscall_func)(uintptr_t)sys_write,
    [SYS_open]          = (syscall_func)(uintptr_t)sys_open,
    [SYS_close]         = (syscall_func)(uintptr_t)sys_close,
    [SYS_stat]          = (syscall_func)(uintptr_t)sys_stat,
    [SYS_fstat]         = (syscall_func)(uintptr_t)sys_fstat,
    [SYS_lseek]         = (syscall_func)(uintptr_t)sys_lseek,
    [SYS_mmap]          = (syscall_func)(uintptr_t)sys_mmap,
    [SYS_munmap]        = (syscall_func)(uintptr_t)sys_munmap,
    [SYS_rt_sigaction]  = (syscall_func)(uintptr_t)sys_rt_sigaction,
    [SYS_rt_sigprocmsk] = (syscall_func)(uintptr_t)sys_rt_sigprocmask,
    [SYS_ioctl]         = (syscall_func)(uintptr_t)sys_ioctl,
    [SYS_access]        = (syscall_func)(uintptr_t)sys_access,
    [SYS_dup]           = (syscall_func)(uintptr_t)sys_dup,
    [SYS_getpid]        = (syscall_func)(uintptr_t)sys_getpid,
    [SYS_clone]         = (syscall_func)(uintptr_t)sys_clone,
    [SYS_execve]        = (syscall_func)(uintptr_t)sys_execve,
    [SYS_exit]          = (syscall_func)(uintptr_t)sys_exit,
    [SYS_wait4]         = (syscall_func)(uintptr_t)sys_wait4,
    [SYS_getuid]        = (syscall_func)(uintptr_t)sys_getuid,
    [SYS_getgid]        = (syscall_func)(uintptr_t)sys_getgid,
    [SYS_geteuid]       = (syscall_func)(uintptr_t)sys_geteuid,
    [SYS_getegid]       = (syscall_func)(uintptr_t)sys_getegid,
    [SYS_getppid]       = (syscall_func)(uintptr_t)sys_getppid,
    [SYS_getpgid]       = (syscall_func)(uintptr_t)sys_getpgid,
    [SYS_arch_prctl]    = (syscall_func)(uintptr_t)sys_arch_prctl,
    [SYS_gettid]        = (syscall_func)(uintptr_t)sys_getpid,
    [SYS_getdents64]    = (syscall_func)(uintptr_t)sys_getdents64,
    [SYS_clock_gettime] = (syscall_func)(uintptr_t)sys_clock_gettime
};

void syscall_handler(struct registers *r) {
    if (r->rax > sizeof syscalls / sizeof(void *) || !syscalls[r->rax]) {
        dprintf("%s:%d: unknown syscall %lu\n", __FILE__, __LINE__, r->rax);
        r->rax = -ENOSYS;
        sched_unlock();
        return;
    }

    syscall_func handler = syscalls[r->rax];
    r->rax = handler(r->rax == SYS_clone ? (long)r : r->rdi, r->rsi, r->rdx, r->rcx, r->r8, r->r9);
}