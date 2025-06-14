#pragma once
#include "kernel/sched.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __x86_64__
#include <kernel/arch/x86_64/vmm.h>
#else
#include <kernel/arch/riscv/mmu.h>
#endif

#define PAGE_SIZE 0x1000
#define HUGE_PAGE_SIZE 0x200000

#define VIRTUAL(ptr) ((void *)((uintptr_t)(ptr) + (uintptr_t)KERNEL_VIRT_BASE))
#define PHYSICAL(ptr) ((void *)((uintptr_t)(ptr) - (uintptr_t)KERNEL_VIRT_BASE))

#define VIRTUAL_IDENT(ptr) ((uintptr_t *)((uintptr_t)(ptr) + (uintptr_t)0xFFFFFFFF80000000))
#define PHYSICAL_IDENT(ptr) ((uintptr_t *)((uintptr_t)(ptr) - (uintptr_t)0xFFFFFFFF80000000))

#define DIV_CEILING(x, y) (x + (y - 1)) / y
#define ALIGN_UP(x, y) (DIV_CEILING(x, y) * y)
#define ALIGN_DOWN(x, y) ((x / y) * y)

extern uintptr_t *kernel_pd;

extern uint64_t mmu_page_count;
extern uint64_t mmu_usable_mem;
extern uint64_t mmu_used_pages;

void *mmu_alloc(size_t page_count);
void  mmu_free(void *ptr, size_t page_count);
void  mmu_map_huge(uintptr_t virt, uintptr_t phys, uint64_t flags);
void  mmu_map(void *virt, void *phys, uint64_t flags);
void  mmu_unmap(void *virt);
void  mmu_mark_used(void *ptr, size_t page_size);
void  mmu_map_pages(size_t count, void *virt, void *phys, uint64_t flags);
void  mmu_unmap_pages(size_t count, void *virt);
void  mmu_destroy_user_pm(uintptr_t *pml4);
uintptr_t mmu_get_physical(uintptr_t *pml4, uintptr_t virt);
uintptr_t *mmu_create_user_pm(struct task *proc);