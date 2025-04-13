#include <stddef.h>
#include <stdatomic.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/vfs.h>
#include <kernel/sched.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/spinlock.h>

#define COM1 0x3f8

atomic_flag serial_lock = ATOMIC_FLAG_INIT;

void serial_install(void) {
    outb(COM1 + 1, 0);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1, 0);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    outb(COM1 + 4, 0x1E);
    outb(COM1, 0x55);

    if (inb(COM1) != 0x55)
        return;

    outb(COM1 + 4, 0x0F);
}

int serial_is_bus_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

int serial_is_data_ready(void) {
    return inb(COM1 + 5) & 0x01;
}

char serial_read_char(void) {
    while (serial_is_data_ready() == 0) {
        sched_yield();
    }
    return inb(COM1);
}

void serial_write_char(char c) {
    while (serial_is_bus_empty() == 0);
    if (c == '\n')
        outb(COM1, '\r');
    outb(COM1, c);
}

void serial_puts(char *str) {
    acquire(&serial_lock);
    while (*str) {
        serial_write_char(*str++);
    }
    release(&serial_lock);
}

int dprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024] = {0};
    int ret = vsprintf(buf, fmt, args);
    serial_puts(buf);
    va_end(args);
    return ret;
}

int32_t serial_write(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len) {
    char *buf = (char *)buffer;
    for (uint32_t i = offset; i < len; i++) {
        serial_write_char(buf[i]);
    }
    return (int32_t)len;
}

int32_t serial_read(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len) {
    char c = serial_read_char();
    memcpy(buffer, &c, 1);
    return 1;
}

void serial_initialize(void) {
    struct vfs_node *serial0 = vfs_create_node("serial0", VFS_CHARDEVICE);
    serial0->write = serial_write;
    serial0->read = serial_read;
    vfs_add_device(serial0);
}