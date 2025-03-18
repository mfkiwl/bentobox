#include <video/vga.h>

void kmain(void *mboot_info) {
    vga_clear();
    vga_enable_cursor();

    vga_puts("Hello, world!\n");

    for (;;) asm ("hlt");
}