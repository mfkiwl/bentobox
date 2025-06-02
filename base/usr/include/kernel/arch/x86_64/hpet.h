#pragma once
#include <stdint.h>
#include <stddef.h>
#include <kernel/acpi.h>

#define HPET_REG_CAP    0x0
#define HPET_REG_CONFIG 0x10
#define HPET_REG_MAIN_COUNTER 0xF0

#define HPET_COMPARATOR_REGS(N) (0x100 + N)
#define HPET_REG_COMPARATOR_CONFIG(N) (0x100 + N * 0x20)
#define HPET_REG_COMPARATOR_VALUE(N) (0x108 + N * 0x20)

extern struct acpi_hpet *hpet;
extern uint32_t hpet_period;

void hpet_install(void);
void hpet_sleep(size_t us);
size_t hpet_get_ticks(void);
void hpet_read_time(long *sec, long *nsec);