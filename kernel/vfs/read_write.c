#include <kernel/arch/x86_64/smp.h>
#include <kernel/sched.h>
#include <kernel/printf.h>

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