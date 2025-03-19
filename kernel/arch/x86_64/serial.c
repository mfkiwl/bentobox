#include <arch/x86_64/io.h>
#include <misc/printf.h>
#include <misc/assert.h>

#define COM1 0x3f8

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