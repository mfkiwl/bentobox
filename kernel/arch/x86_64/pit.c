#include <stdint.h>
#include <stddef.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/arch/x86_64/pit.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/printf.h>

#define PIT_FREQ  1000
#define TIMER_IRQ 0

size_t pit_ticks = 0;

static void pit_handler(struct registers *r) {
    pit_ticks++;
    lapic_eoi();
}

static void pit_set_timer_phase(size_t hz) {
    uint16_t div = (uint16_t)(1193180 / hz);
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)div);
    outb(0x40, (uint8_t)(div >> 8));

    dprintf("%s:%d: set timer phase to %dhz\n", __FILE__, __LINE__, hz);
}

void pit_install(void) {
    pit_set_timer_phase(PIT_FREQ);
    irq_register(TIMER_IRQ, pit_handler);
}

void pit_sleep(size_t ms) {
    size_t start_ticks = pit_ticks;
    size_t end_ticks = start_ticks + ms;

    while (pit_ticks < end_ticks) {
        asm volatile ("hlt");
    }
}