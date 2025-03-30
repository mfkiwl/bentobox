#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/arch/x86_64/ioapic.h>
#include <stddef.h>
#include <kernel/mmu.h>
#include <kernel/acpi.h>
#include <kernel/printf.h>

struct acpi_hpet *hpet = NULL;
uint32_t hpet_period = 0;

__attribute__((no_sanitize("undefined")))
uint64_t hpet_read(uint32_t reg) {
    return *((uint64_t*)(VIRTUAL(hpet->address) + reg));
}

__attribute__((no_sanitize("undefined")))
void hpet_write(uint32_t reg, uint64_t value) {
    *((uint64_t*)(VIRTUAL(hpet->address) + reg)) = value;
}

uint64_t hpet_get_ticks(void) {
    return hpet_read(HPET_REG_MAIN_COUNTER);
}

size_t last_second = -1, second = 0, ticks = 0;

void hpet_irq_handler(struct registers *r) {
    second = hpet_get_ticks() / 100000000;
    if (last_second != second) {
        last_second = second;
        printf("\r%lu", ticks);
        ticks = 0;
    }
    ticks++;
    lapic_eoi();
}

void arm_hpet_interrupt_timer(int n, size_t femtos) {
    uint64_t config = hpet_read(HPET_REG_CONFIG);
    hpet_write(HPET_REG_CONFIG, config & ~0x1);
    
    uint64_t timer_config = hpet_read(HPET_COMPARATOR_REGS(n));
    bool periodic_capable = (timer_config >> 4) & 0x1;
    
    timer_config &= ~0x3F14;
    timer_config |= (1 << 2);
    
    if (periodic_capable) {
        timer_config |= (1 << 3);
        timer_config |= (1 << 6);
    } else {
        dprintf("%s:%d: warning: periodic mode unsupported on HPET %d\n", __FILE__, __LINE__, n);
    }
    
    uint32_t allowed_routes = timer_config >> 32;
    size_t used_route = 0;
    while ((allowed_routes & 1) == 0 && used_route < 32) {
        used_route++;
        allowed_routes >>= 1;
    }
    
    timer_config &= ~(0x1F << 9);
    timer_config |= (used_route << 9);
    
    hpet_write(HPET_COMPARATOR_REGS(n), timer_config);
    
    dprintf("%s:%d: using interrupt route %lu\n", __FILE__, __LINE__, used_route);
    
    irq_register(used_route, hpet_irq_handler);
    
    uint64_t ticks = femtos / hpet_period;
    
    dprintf("%s:%d: calibrating timer to %luus\n", __FILE__, __LINE__, femtos / 1000000); 
    
    hpet_write(HPET_REG_MAIN_COUNTER, 0);
    hpet_write(HPET_COMPARATOR_REGS(n) + 8, ticks);
    hpet_write(HPET_REG_CONFIG, config | 0x1);
}

void hpet_install(void) {
    hpet = acpi_find_table("HPET");

    if (!hpet) {
        printf("%s:%d: HPET not found\n", __FILE__, __LINE__);
        return;
    }

    mmu_map((uintptr_t)VIRTUAL(hpet->address), hpet->address, PTE_PRESENT | PTE_WRITABLE);
    
    uint64_t cap = hpet_read(HPET_REG_CAP);
    hpet_period = cap >> 32;

    hpet_write(HPET_REG_CONFIG, 0x0);
    hpet_write(HPET_REG_MAIN_COUNTER, 0);
    
    arm_hpet_interrupt_timer(0, 1000000000000);
    
    for (;;) {
        //printf("\rHPET timer ticks: %lu", hpet_read(HPET_REG_MAIN_COUNTER));
    }
}