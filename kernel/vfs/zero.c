#include <stdint.h>
#include <stddef.h>
#include <kernel/vfs.h>
#include <kernel/string.h>

struct vfs_node *zero_dev = NULL;
struct vfs_node *null_dev = NULL;

int32_t zero_read(struct vfs_node *node, void *buffer, uint32_t len) {
    memset(buffer, 0, len);
    return (int32_t)len;
}

int32_t null_read(struct vfs_node *node, void *buffer, uint32_t len) {
    return 0;
}

void zero_initialize(void) {
    zero_dev = vfs_create_node("zero", VFS_CHARDEVICE);
    zero_dev->read = zero_read;
    vfs_add_node(vfs_dev, zero_dev);

    null_dev = vfs_create_node("null", VFS_CHARDEVICE);
    null_dev->read = null_read;
    vfs_add_node(vfs_dev, null_dev);
}