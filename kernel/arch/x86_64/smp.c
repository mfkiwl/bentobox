#include <kernel/arch/x86_64/pit.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/mmu.h>
#include <kernel/acpi.h>
#include <kernel/string.h>
#include <kernel/printf.h>

extern void _L8000_ap_trampoline();

volatile uint8_t ap_started = 0, ap_running = 0;

/*
 * https://wiki.osdev.org/Symmetric_Multiprocessing
 */
void smp_initialize(void) {
    uint8_t bspid, bspdone = 0;      // BSP id and spinlock flag
    // get the BSP's Local APIC ID
    asm volatile ("mov $1, %%eax; cpuid; shrl $24, %%ebx;": "=b"(bspid) : :);

    printf("Current core BSP ID: %d\n", bspid);
    printf("LAPIC address: %x\n", LAPIC_REGS);

    memcpy((void*)0x8000, &_L8000_ap_trampoline, PAGE_SIZE);

    for (uint32_t i = 0; i < madt_lapics; i++) {
        if (madt_lapic_list[i]->id == bspid)
            continue; /* skip BSP, that's already running this code */

        /* send INIT IPI */
        lapic_write(LAPIC_ESR, 0);                                                   /* clear APIC errors */
        lapic_write(LAPIC_ICRHI, i << LAPIC_ICDESTSHIFT);                            /* select AP */
        lapic_write(LAPIC_ICRLO, (lapic_read(LAPIC_ICRLO) & 0xfff00000) | 0x00C500); /* trigger INIT IPI */
        do {
            asm volatile ("pause" : : : "memory");                                   /* wait for delivery */
        } while (lapic_read(LAPIC_ICRLO) & (1 << 12));
        lapic_write(LAPIC_ICRHI, i << LAPIC_ICDESTSHIFT);                            /* select AP */
        lapic_write(LAPIC_ICRLO, (lapic_read(LAPIC_ICRLO) & 0xfff00000) | 0x008500); /* deassert */
        do {
            asm volatile ("pause" : : : "memory");                                   /* wait for delivery */
        } while (lapic_read(LAPIC_ICRLO) & (1 << 12));

        pit_sleep(10);

        /* send STARTUP IPI (twice) */
        for(int j = 0; j < 2; j++) {
            lapic_write(LAPIC_ESR, 0);                                                   /* clear APIC errors */
            lapic_write(LAPIC_ICRHI, i << LAPIC_ICDESTSHIFT);                            /* select AP */
            lapic_write(LAPIC_ICRLO, (lapic_read(LAPIC_ICRLO) & 0xfff0f800) | 0x000608); /* trigger STARTUP IPI for 0x0800:0x0000 */
            pit_sleep(1);
            do {
                asm volatile ("pause" : : : "memory");
            } while (lapic_read(LAPIC_ICRLO) & (1 << 12)); // wait for delivery
        }
    }
    // release the AP spinlocks
    bspdone = 1;
    // now you'll have the number of running APs in 'aprunning'

    printf("%s:%d: started %d cores\n", __FILE__, __LINE__, madt_lapics);
}

void ap_startup(void) {}