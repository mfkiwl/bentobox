#include <arch/x86_64/gdt.h>
#include <arch/x86_64/serial.h>
#include <misc/printf.h>
#include <video/vga.h>
#include <sys/version.h>

void kmain(void *mboot_info) {
    vga_clear();
    vga_enable_cursor();

    printf("%s %d.%d.%d %s %s %s\n",
        __kernel_name, __kernel_version_major, __kernel_version_minor,
        __kernel_version_patch, __kernel_build_date, __kernel_build_time,
        __kernel_arch);

    serial_install();
    gdt_install();
}