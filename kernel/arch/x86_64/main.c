#include <limine.h>
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/arch/x86_64/tss.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/ps2.h>
#include <kernel/arch/x86_64/vga.h>
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/arch/x86_64/ioapic.h>
#include <kernel/arch/x86_64/serial.h>
#include <kernel/mmu.h>
#include <kernel/pci.h>
#include <kernel/lfb.h>
#include <kernel/acpi.h>
#include <kernel/elf64.h>
#include <kernel/sched.h>
#include <kernel/malloc.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/assert.h>
#include <kernel/version.h>
#include <kernel/flanterm.h>
#include <kernel/spinlock.h>

LIMINE_BASE_REVISION(1)

struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 1
};

extern void generic_startup(void);
extern void generic_main(void);

atomic_flag flanterm_lock = ATOMIC_FLAG_INIT;

void putchar(char c) {
	if (!ft_ctx) {
		serial_write_char(c);
	} else {
		flanterm_write(ft_ctx, &c, 1);
	}
}

void puts(char *s) {
	if (!ft_ctx) {
		serial_puts(s);
	} else {
		acquire(&flanterm_lock);
		flanterm_write(ft_ctx, s, strlen(s));
		release(&flanterm_lock);
	}
}

void generic_fatal(void) {
	for (uint32_t i = 0; i < madt_lapics; i++) {
		if (i == this_core()->lapic_id) continue;
		lapic_ipi(i, 0x447D);
	}
	asm ("cli");
	for (;;) asm ("hlt");
}

void generic_load_modules(void) {
	if (module_request.response == NULL) {
        printf("Modules not passed\n");
        return;
    }
    struct limine_module_response *module_response = module_request.response;
    printf("Modules feature, revision %d\n", module_response->revision);
    printf("%d module(s)\n", module_response->module_count);

	for (size_t i = 1; i < module_request.response->module_count; i++) {
		elf_module(module_request.response->modules[i]);
	}
}

void kmain(void) {
    serial_install();
    
    dprintf("%s %d.%d-%s %s %s %s\n",
        __kernel_name, __kernel_version_major, __kernel_version_minor,
		__kernel_commit_hash, __kernel_build_date, __kernel_build_time, __kernel_arch);

	lfb_initialize();
    gdt_install();
    idt_install();
	pmm_install();
	tss_install();
	vmm_install();
	kernel_heap = heap_create();

	printf("\n  \033[97mStarting up \033[94mbentobox (%s)\033[0m\n\n", __kernel_arch);

	elf_module(module_request.response->modules[0]);
	acpi_install();
	lapic_install();
	ioapic_install();
	ps2_install();
	hpet_install();
	lapic_calibrate_timer();
	smp_initialize();

	generic_startup();
	generic_main();
}