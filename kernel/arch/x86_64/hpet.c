#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/arch/x86_64/ioapic.h>
#include <stddef.h>
#include <kernel/mmu.h>
#include <kernel/acpi.h>
#include <kernel/panic.h>
#include <kernel/printf.h>

uint64_t hpet_address = 0;
uint32_t hpet_period = 0;

__attribute__((no_sanitize("undefined")))
uint64_t hpet_read(uint32_t reg) {
    return *((uint64_t*)(VIRTUAL(hpet_address) + reg));
}

__attribute__((no_sanitize("undefined")))
void hpet_write(uint32_t reg, uint64_t value) {
    *((uint64_t*)(VIRTUAL(hpet_address) + reg)) = value;
}

size_t hpet_get_ticks(void) {
    return hpet_read(HPET_REG_MAIN_COUNTER);
}

void hpet_sleep(size_t us) {
    size_t end_ticks = hpet_read(HPET_REG_MAIN_COUNTER) + us * 1000000000 / hpet_period;

    while (hpet_read(HPET_REG_MAIN_COUNTER) < end_ticks) {
        asm ("pause" : : : "memory");
    }
}

void hpet_install(void) {
    struct acpi_hpet *hpet = acpi_find_table("HPET");

    if (hpet) {
        hpet_address = (uint64_t)VIRTUAL(hpet->address);
        mmu_map(hpet_address, hpet->address, PTE_PRESENT | PTE_WRITABLE);
        uint64_t cap = hpet_read(HPET_REG_CAP);
        hpet_period = (cap >> 32);
    } else {
        /* You ARE there. */
        hpet_address = 0xFED00000;
        hpet_period = 10000000;
    }

    dprintf("%s:%d: 1us is %lu ticks\n", __FILE__, __LINE__, hpet_period * 1 / 1000000);

    hpet_write(HPET_REG_CONFIG, 0x0);
    hpet_write(HPET_REG_MAIN_COUNTER, 0);
    hpet_write(HPET_REG_CONFIG, 0x1);

    dprintf("%s:%d: enabled HPET\n", __FILE__, __LINE__);
}