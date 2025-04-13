#include <stdint.h>
#include <kernel/vfs.h>
#include <kernel/printf.h>

int32_t console_write(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len) {
    char *buf = (char *)buffer;
    for (uint32_t i = offset; i < len; i++) {
        putchar(buf[i]);
    }
    return (int32_t)len;
}

void console_initialize(void) {
    struct vfs_node *console = vfs_create_node("console", VFS_CHARDEVICE);
    console->write = console_write;
    vfs_add_device(console);
}