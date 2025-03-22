#include <stddef.h>
#include <kernel/multiboot.h>
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/apic.h>
#include <kernel/arch/x86_64/serial.h>
#include <kernel/mmu.h>
#include <kernel/acpi.h>
#include <kernel/heap.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/assert.h>
#include <kernel/video/vga.h>
#include <kernel/version.h>

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

void generic_fatal(void) {
	asm ("cli");
	for (;;) asm ("hlt");
}

void kmain(void *mboot_info, uint32_t mboot_magic) {
    vga_clear();
    vga_enable_cursor();

    serial_install();
    
    dprintf("%s %d.%d %s %s %s\n",
        __kernel_name, __kernel_version_major, __kernel_version_minor,
		__kernel_build_date, __kernel_build_time, __kernel_arch);

    assert(mboot_magic == 0x36d76289);
    gdt_install();
    idt_install();
	pmm_install(mboot_info);
	vmm_install();
	kernel_heap = heap_create();

	acpi_install();
	lapic_install();

	printf("Welcome to bentobox v%d.%d (%s %s %s)!\n",
		__kernel_version_major, __kernel_version_minor,
		__kernel_build_date, __kernel_build_time, __kernel_arch);
}