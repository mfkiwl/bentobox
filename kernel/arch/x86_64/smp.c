#include <stdatomic.h>
#include <kernel/arch/x86_64/pit.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/sys/spinlock.h>
#include <kernel/mmu.h>
#include <kernel/acpi.h>
#include <kernel/heap.h>
#include <kernel/string.h>
#include <kernel/printf.h>

extern void _L8000_ap_trampoline();

volatile uint8_t smp_running_cpus = 1;
atomic_flag smp_lock = ATOMIC_FLAG_INIT;

struct cpu *smp_cpu_list[32];

/*
 * https://wiki.osdev.org/Symmetric_Multiprocessing
 */
void smp_initialize(void) {
    uint8_t bspid;
    asm volatile ("mov $1, %%eax; cpuid; shrl $24, %%ebx;": "=b"(bspid) : :); /* get the BSP's LAPIC ID */

    memcpy((void*)0x8000, &_L8000_ap_trampoline, PAGE_SIZE);

    for (uint32_t i = 0; i < madt_lapics; i++) {
        if (madt_lapic_list[i]->id == bspid)
            continue; /* skip BSP, that's already running this code */

        acquire(&smp_lock);

        struct cpu *core = (struct cpu *)PHYSICAL(kmalloc(sizeof(struct cpu)));
        core->id = i;
        smp_cpu_list[i] = core;

        /* send INIT IPI */
        lapic_write(LAPIC_ESR, 0);                                                        /* clear APIC errors */
        lapic_write(LAPIC_ICRHI, i << LAPIC_ICDESTSHIFT);                                 /* select AP */
        lapic_write(LAPIC_ICRLO, (lapic_read(LAPIC_ICRLO) & 0xfff00000) | 0x00C500); /* trigger INIT IPI */
        do {
            asm volatile ("pause" : : : "memory");                                                   /* wait for delivery */
        } while (lapic_read(LAPIC_ICRLO) & (1 << 12));
        lapic_write(LAPIC_ICRHI, i << LAPIC_ICDESTSHIFT);                                 /* select AP */
        lapic_write(LAPIC_ICRLO, (lapic_read(LAPIC_ICRLO) & 0xfff00000) | 0x008500); /* deassert */
        do {
            asm volatile ("pause" : : : "memory");                                                   /* wait for delivery */
        } while (lapic_read(LAPIC_ICRLO) & (1 << 12));

        pit_sleep(10);

        /* send STARTUP IPI (twice) */
        for(int j = 0; j < 2; j++) {
            lapic_write(LAPIC_ESR, 0);                                                        /* clear APIC errors */
            lapic_write(LAPIC_ICRHI, i << LAPIC_ICDESTSHIFT);                                 /* select AP */
            lapic_write(LAPIC_ICRLO, (lapic_read(LAPIC_ICRLO) & 0xfff0f800) | 0x000608); /* trigger STARTUP IPI for 0x0800:0x0000 */
            pit_sleep(1);
            do {
                asm volatile ("pause" : : : "memory");                                                   /* wait for delivery */
            } while (lapic_read(LAPIC_ICRLO) & (1 << 12));
        }

        acquire(&smp_lock);
        release(&smp_lock);
    }

    dprintf("%s:%d: started %d processors\n", __FILE__, __LINE__, smp_running_cpus);
    printf("\033[92m * \033[97mInitialized SMP with %d CPUs\033[0m\n", smp_running_cpus);
}

struct cpu *this_core(void) {
    uint8_t bspid;
    asm volatile ("mov $1, %%eax; cpuid; shrl $24, %%ebx;": "=b"(bspid) : :);
    return smp_cpu_list[bspid];
}

void ap_startup(void) {
    smp_running_cpus++;
    release(&smp_lock);

    uint64_t id = this_core()->id;

    printf("Hello from CPU %d!\n", id);
    for (;;);
}