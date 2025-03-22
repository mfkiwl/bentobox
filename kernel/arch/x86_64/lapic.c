#include <cpuid.h>
#include <kernel/acpi.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/arch/x86_64/apic.h>
#include <kernel/mmu.h>
#include <kernel/printf.h>
#include <kernel/assert.h>

uint32_t lapic_ticks = 0;

static bool cpu_check_apic(void) {
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx))
        return edx & CPUID_FEAT_EDX_APIC;
    return false;
}

__attribute__((no_sanitize("undefined")))
uint32_t lapic_read(uint32_t reg) {
    return *((uint32_t *)((uintptr_t)(LAPIC_REGS) + reg));
}

__attribute__((no_sanitize("undefined")))
void lapic_write(uint32_t reg, uint32_t value) {
    *((uint32_t *)((uintptr_t)(LAPIC_REGS) + reg)) = value;
}

void lapic_stop_timer(void) {
    lapic_write(LAPIC_TIMER_INITCNT, 0);
    lapic_write(LAPIC_TIMER_LVT, LAPIC_TIMER_DISABLE);
}

void lapic_oneshot(uint8_t vector, uint32_t ms) {
    lapic_stop_timer();

    lapic_write(LAPIC_TIMER_DIV, 0);
    lapic_write(LAPIC_TIMER_LVT, vector);
    lapic_write(LAPIC_TIMER_INITCNT, lapic_ticks * ms);
}

void lapic_eoi(void) {
    lapic_write((uint8_t)LAPIC_EOI, 0);
}

void lapic_ipi(uint32_t id, uint8_t irq) {
    lapic_write(LAPIC_ICRHI, id << LAPIC_ICDESTSHIFT);
    lapic_write(LAPIC_ICRLO, irq);
}

void lapic_install(void) {
    assert(acpi_root_sdt);
    assert(cpu_check_apic());

    mmu_map(LAPIC_REGS, (uintptr_t)VIRTUAL(LAPIC_REGS), PTE_PRESENT | PTE_WRITABLE);
    lapic_write(LAPIC_SIV, lapic_read(LAPIC_SIV) | 0x100);

    dprintf("%s:%d: initialized Local APIC\n", __FILE__, __LINE__);
}