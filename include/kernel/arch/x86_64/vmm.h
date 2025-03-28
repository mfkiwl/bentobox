#pragma once
#include <stdint.h>

#define PTE_ADDR_MASK 0x000ffffffffff000
#define PTE_GET_ADDR(x) ((x) & PTE_ADDR_MASK)
#define PTE_GET_FLAGS(x) ((x) & ~PTE_ADDR_MASK)

#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000
#define KERNEL_PHYS_BASE 0x100000

extern uintptr_t *pml;
extern uintptr_t *kernel_pd;

void pmm_install(void *mboot_info);
void vmm_install(void);
void vmm_switch_pm(uintptr_t *pm);