#include <stddef.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/vfs.h>
#include <kernel/printf.h>
#include <kernel/assert.h>

#define COM1 0x3f8

struct vfs_node *serial_dev = NULL;

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

    assert(inb(COM1) == 0x55);

    outb(COM1 + 4, 0x0F);
    dprintf("%s:%d: enabled port 0x3f8\n", __FILE__, __LINE__);
}

int serial_is_bus_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

int serial_is_data_ready(void) {
    return inb(COM1 + 5) & 0x01;
}

char serial_read_char(void) {
    while (serial_is_data_ready() == 0);
    return inb(COM1);
}

void serial_write_char(char c) {
    while (serial_is_bus_empty() == 0);
    if (c == '\n')
        outb(COM1, '\r');
    outb(COM1, c);
}

void serial_puts(char *str) {
    while (*str) {
        serial_write_char(*str++);
    }
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

int32_t serial_write(struct vfs_node *node, void *buffer, uint32_t len) {
    char *buf = (char *)buffer;
    for (uint32_t i = 0; i < len; i++) {
        serial_write_char(buf[i]);
    }
    return (int32_t)len;
}

int32_t serial_read(struct vfs_node *node, void *buffer, uint32_t len) {
    char *buf = (char *)buffer;
    uint32_t i = 0;

    while (i < len) {
        while (!serial_is_data_ready());

        buf[i] = serial_read_char();

        if (buf[i] == '\r') {
            serial_write_char('\n');
            buf[i] = '\0';
            return (int32_t)i;
        } else if (buf[i] == 127) {
            if (i > 0) {
                serial_write_char('\b');
                serial_write_char(' ');
                serial_write_char('\b');
                i--;
            }
        } else {
            serial_write_char(buf[i]);
            i++;
        }
    }

    return (int32_t)i;
}

void serial_tty_install(void) {
    serial_dev = vfs_create_node("serial0", VFS_CHARDEVICE);
    serial_dev->write = serial_write;
    serial_dev->read = serial_read;
    vfs_add_node(vfs_dev, serial_dev);
}