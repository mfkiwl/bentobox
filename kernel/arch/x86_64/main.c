#include <stddef.h>
#include <multiboot.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/mmu.h>
#include <arch/x86_64/serial.h>
#include <misc/printf.h>
#include <misc/assert.h>
#include <video/vga.h>
#include <sys/version.h>

void *mboot2_find_next(char *current, uint32_t type) {
	char *header = current;
	while ((uintptr_t)header & 7) header++;
	struct multiboot_tag *tag = (void *)header;
	while (1) {
		if (tag->type == 0) return NULL;
		if (tag->type == type) return tag;

		header += tag->size;
		while ((uintptr_t)header & 7) header++;
		tag = (void*)header;
	}
}

void *mboot2_find_tag(void *base, uint32_t type) {
	char *header = base;
	header += 8;
	return mboot2_find_next(header, type);
}

void kmain(void *mboot_info) {
    vga_clear();
    vga_enable_cursor();

    serial_install();
    
    dprintf("%s %d.%d %s %s %s\n",
        __kernel_name, __kernel_version_major, __kernel_version_minor,
		__kernel_build_date, __kernel_build_time, __kernel_arch);

    gdt_install();
    idt_install();
	mmu_init(mboot_info);

	printf("Welcome to bentobox v%d.%d (%s %s %s)!\n",
		__kernel_version_major, __kernel_version_minor,
		__kernel_build_date, __kernel_build_time, __kernel_arch);
}