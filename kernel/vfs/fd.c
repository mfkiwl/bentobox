#include <kernel/fd.h>
#include <kernel/vfs.h>
#include <kernel/sched.h>
#include <kernel/malloc.h>

struct fd fd_new(struct vfs_node *node, int flags) {
    struct fd fd;
    fd.node = node;
    fd.flags = flags;
    fd.offset = 0;
    return fd;
}

int fd_open(const char *path, int flags) {
    struct vfs_node *node = vfs_open(NULL, path);
    if (!node) return -1;

    for (size_t i = 0; i < sizeof this->fd_table / sizeof(struct fd); i++) {
        if (!this->fd_table[i].node) {
            this->fd_table[i] = fd_new(node, flags);
            return i;
        }
    }
    vfs_close(node);
    return -1;
}