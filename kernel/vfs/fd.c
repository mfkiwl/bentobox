#include <kernel/fd.h>
#include <kernel/malloc.h>

struct fd fd_open(struct vfs_node *node, uint16_t flags) {
    struct fd fd;
    fd.node = node;
    fd.flags = flags;
    fd.offset = 0;
    return fd;
}