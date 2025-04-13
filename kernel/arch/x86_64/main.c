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
#include <kernel/multiboot.h>

extern void generic_startup(void);
extern void generic_main(void);

static void *mboot = NULL;

atomic_flag flanterm_lock = ATOMIC_FLAG_INIT;

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

void mboot2_load_modules(void *base) {
	struct multiboot_tag_module *mod = mboot2_find_tag(base, MULTIBOOT_TAG_TYPE_MODULE);
    while (mod) {
		if (strcmp(mod->string, "ksym")) elf_module(mod);
        mod = mboot2_find_next((char *)mod + ALIGN_UP(mod->size, 8), MULTIBOOT_TAG_TYPE_MODULE);
    }
}

void putchar(char c) {
	if (!ft_ctx) {
		vga_putchar(c);
	} else {
		flanterm_write(ft_ctx, &c, 1);
	}
}

void puts(char *s) {
	if (!ft_ctx) {
		vga_puts(s);
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
	assert(mboot);
	mboot2_load_modules(mboot);
}

void generic_map_kernel(uintptr_t *pml4) {
    this_core()->pml4 = pml4;
	pml4[511] = kernel_pd[511];
    mmu_map_pages(16383, 0x1000, 0x1000, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    mmu_map((uintptr_t)VIRTUAL(LAPIC_REGS), LAPIC_REGS, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    mmu_map((uintptr_t)VIRTUAL(hpet->address), hpet->address, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    mmu_map((uintptr_t)ALIGN_DOWN((uintptr_t)hpet, PAGE_SIZE), (uintptr_t)ALIGN_DOWN((uintptr_t)hpet, PAGE_SIZE), PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    mmu_map_pages((ALIGN_UP((lfb.pitch * lfb.height), PAGE_SIZE) / PAGE_SIZE), (uintptr_t)lfb.addr, (uintptr_t)lfb.addr, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
}

void kmain(void *mboot_info, uint32_t mboot_magic) {
    serial_install();
    
    dprintf("%s %d.%d-%s %s %s %s\n",
        __kernel_name, __kernel_version_major, __kernel_version_minor,
		__kernel_commit_hash, __kernel_build_date, __kernel_build_time, __kernel_arch);

    assert(mboot_magic == 0x36d76289);
	mboot = mboot_info;
    gdt_install();
    idt_install();
	pmm_install(mboot_info);
	tss_install();
	vmm_install();
	kernel_heap = heap_create();

	lfb_initialize(mboot_info);

	printf("\n  \033[97mStarting up \033[94mbentobox (%s)\033[0m\n\n", __kernel_arch);

	elf_module(mboot2_find_tag(mboot_info, MULTIBOOT_TAG_TYPE_MODULE));
	acpi_install(mboot_info);
	lapic_install();
	ioapic_install();
	ps2_install();
	hpet_install();
	lapic_calibrate_timer();
	smp_initialize();

	generic_startup();
	generic_main();
}