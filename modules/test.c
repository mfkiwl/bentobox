#include <kernel/arch/x86_64/io.h>

void _start() {
    outb(0x3f8, 'H');
    outb(0x3f8, 'i');
    outb(0x3f8, '!');
    for (;;);
}