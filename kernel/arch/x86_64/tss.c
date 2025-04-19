#include <stdint.h>
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/arch/x86_64/tss.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/spinlock.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/sched.h>
#include <kernel/mmu.h>

struct tss_entry tss;

atomic_flag tss_lock = ATOMIC_FLAG_INIT;

void write_tss(int index, uint64_t rsp0) {
    acquire(&tss_lock);
    gdt_set_entry(index, sizeof(struct tss_entry), (uint64_t)&tss, 0x89, 0x20);
    
    memset(&tss, 0, sizeof(struct tss_entry));
    tss.rsp0 = rsp0;

    asm volatile ("mov $0x28, %%ax; ltr %%ax" : : : "ax");
    release(&tss_lock);
}

void tss_install(void) {
    write_tss(5, (uint64_t)mmu_alloc(1) + PAGE_SIZE);
    dprintf("%s:%d: initialized TSS on CPU #%d\n", __FILE__, __LINE__, this_core()->id);
}

void set_kernel_stack(uint64_t stack) {
    tss.rsp0 = stack;
}