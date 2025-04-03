#include <stdint.h>
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/arch/x86_64/tss.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/sys/sched.h>

struct tss_entry tss[SMP_MAX_CORES];

extern void flush_tss();
extern void *stack_top;

void write_tss(int core, int index, uint64_t rsp0) {
    uint64_t base = (uint64_t)&tss[core];
    uint32_t limit = base + sizeof(struct tss_entry) - 1;

    gdt_set_entry(index, limit, base, 0b10001001, 0b00100000);
    
    memset(&tss[core], 0, sizeof(struct tss_entry));
    tss[core].rsp0 = rsp0;

    flush_tss();
}

void tss_install(void) {
    write_tss(this_core()->id, 5, (uint64_t)stack_top);
    dprintf("%s:%d: initialized TSS on CPU #%d\n", __FILE__, __LINE__, this_core()->id);
}

void set_kernel_stack(uint64_t stack) {
    tss[this_core()->id].rsp0 = stack;
}