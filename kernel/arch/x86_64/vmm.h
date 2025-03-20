#pragma once
#include <stdint.h>

#define PAGE_SIZE 4096

#define PTE_PRESENT 1ul
#define PTE_WRITABLE 2ul
#define PTE_USER 4ul
#define PTE_NX (1ul << 63)

#define PTE_ADDR_MASK 0x000ffffffffff000
#define PTE_GET_ADDR(x) ((x) & PTE_ADDR_MASK)
#define PTE_GET_FLAGS(x) ((x) & ~PTE_ADDR_MASK)

#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000
#define KERNEL_PHYS_BASE 0x100000

#define VIRTUAL(ptr) ((void *)((uintptr_t)ptr) + KERNEL_VIRT_BASE - KERNEL_PHYS_BASE)
#define PHYSICAL(ptr) ((void *)((uintptr_t)ptr) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)

#define DIV_CEILING(x, y) (x + (y - 1)) / y
#define ALIGN_UP(x, y) DIV_CEILING(x, y) * y
#define ALIGN_DOWN(x, y) (x / y) * y

void vmm_map(uintptr_t virt, uintptr_t phys, uint64_t flags);
void vmm_unmap(uintptr_t virt);
void vmm_install(void);