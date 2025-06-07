#include <kernel/vfs.h>
#include <kernel/printf.h>
#include <kernel/module.h>
#include <kernel/assert.h>

long tmpfs_write(struct vfs_node *node, void *buffer, long offset, size_t len) {
    unimplemented;
    return 0;
}

int init() {
    dprintf("%s:%d: starting tmpfs module\n", __FILE__, __LINE__);

    struct vfs_node *tmp = vfs_create_node("tmp", VFS_DIRECTORY);
    vfs_add_node(NULL, tmp);
    return 0;
}

int fini() {
    dprintf("%s:%d: Goodbye!\n", __FILE__, __LINE__);
    return 0;
}

struct Module metadata = {
    .name = "ext2 driver",
    .init = init,
    .fini = fini
};