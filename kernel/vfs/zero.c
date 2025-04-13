#include <stdint.h>
#include <stddef.h>
#include <kernel/vfs.h>
#include <kernel/string.h>

int32_t zero_read(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len) {
    memset(buffer, 0, len);
    return (int32_t)len;
}

int32_t null_read(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len) {
    return 0;
}

void zero_initialize(void) {
    struct vfs_node *zero = vfs_create_node("zero", VFS_CHARDEVICE);
    zero->read = zero_read;
    vfs_add_device(zero);

    struct vfs_node *null = vfs_create_node("null", VFS_CHARDEVICE);
    null->read = null_read;
    vfs_add_device(null);
}