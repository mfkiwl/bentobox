#include <stddef.h>
#include <stdatomic.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/vfs.h>
#include <kernel/fifo.h>
#include <kernel/sched.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/spinlock.h>

#define COM1 0x3f8

atomic_flag serial_lock = ATOMIC_FLAG_INIT;
uint16_t serial_base = COM1;
struct fifo serial_fifo;

void serial_install(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 0, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    outb(COM1 + 4, 0x1E);
    outb(COM1 + 0, 0x55);

    if (inb(COM1) != 0x55) {
        serial_base = 0;
        return;
    }

    outb(COM1 + 4, 0x0F);
}

int serial_is_bus_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

int serial_is_data_ready(void) {
    return inb(COM1 + 5) & 0x01;
}

char serial_read_char(void) {
    int c = 0;
    while (!fifo_dequeue(&serial_fifo, &c)) {
        sched_yield();
    }
    return c;
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
    if (serial_base == COM1) {
        serial_puts(buf);
    } else {
        puts(buf);
    }
    va_end(args);
    return ret;
}

long serial_write(struct vfs_node *node, void *buffer, long offset, size_t len) {
    char *buf = (char *)buffer;
    for (uint32_t i = 0; i < len; i++) {
        serial_write_char(buf[i]);
    }
    return (int32_t)len;
}

long serial_read(struct vfs_node *node, void *buffer, long offset, size_t len) {
    size_t i = 0;
    char *str = buffer;
    while (i < len) {
        str[i] = serial_read_char();

        switch (str[i]) {
            case '\0':
                break;
            case '\n':
            case '\r':
                fprintf(stdout, "\n");
                str[i++] = '\n';
                str[i] = '\0';
                return i;
            case '\b':
            case 127:
                if (i > 0) {
                    fprintf(stdout, "\b \b");
                    str[i] = '\0';
                    i--;
                }
                break;
            default:
                fprintf(stdout, "%c", str[i]);
                i++;
                break;
        }
    }

    return i;
}

void irq4_handler(struct registers *r) {
    uint8_t iir = inb(COM1 + 2);
    
    if ((iir & 0x06) == 0x04) {
        int c = inb(COM1);
        fifo_enqueue(&serial_fifo, c);

        if (c == '`') {
            serial_puts("\033[H\033[J");
        }
    }
    
    lapic_eoi();
}

void serial_initialize(void) {
    fifo_init(&serial_fifo, 64);
    irq_register(4, irq4_handler);
    outb(COM1 + 1, 0x01);

    struct vfs_node *serial0 = vfs_create_node("serial0", VFS_CHARDEVICE);
    serial0->write = serial_write;
    serial0->read = serial_read;
    vfs_add_device(serial0);
}