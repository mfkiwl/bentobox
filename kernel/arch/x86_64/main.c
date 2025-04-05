#include <stddef.h>
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/arch/x86_64/tss.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/vga.h>
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/arch/x86_64/ioapic.h>
#include <kernel/arch/x86_64/serial.h>
#include <kernel/mmu.h>
#include <kernel/pci.h>
#include <kernel/lfb.h>
#include <kernel/acpi.h>
#include <kernel/heap.h>
#include <kernel/sched.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/assert.h>
#include <kernel/version.h>
#include <kernel/flanterm.h>
#include <kernel/multiboot.h>

extern void generic_startup(void);
extern void generic_main(void);

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

void puts(char *s) {
	if (!ft_ctx) {
		vga_puts(s);
	} else {
		flanterm_write(ft_ctx, s, strlen(s));
	}
}

void generic_fatal(void) {
	for (uint32_t i = 0; i < madt_lapics; i++) {
		lapic_ipi(i, 0x447D);
	}
	asm ("cli");
	for (;;) asm ("hlt");
}

void generic_pause(void) {
	__builtin_ia32_pause();
}

void mubsan_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    asm volatile ("cli");
    for (;;) asm volatile ("hlt");
}

void kmain(void *mboot_info, uint32_t mboot_magic) {
    serial_install();
    
    dprintf("%s %d.%d %s %s %s\n",
        __kernel_name, __kernel_version_major, __kernel_version_minor,
		__kernel_build_date, __kernel_build_time, __kernel_arch);

    assert(mboot_magic == 0x36d76289);
    gdt_install();
    idt_install();
	tss_install();
	pmm_install(mboot_info);
	vmm_install();
	kernel_heap = heap_create();

	lfb_initialize(mboot_info);

	printf("\n  \033[97mStarting up \033[94mbentobox (%s)\033[0m\n\n", __kernel_arch);

	acpi_install(mboot_info);
	lapic_install();
	ioapic_install();
	hpet_install();
	lapic_calibrate_timer();
	smp_initialize();

	generic_startup();
	generic_main();
}