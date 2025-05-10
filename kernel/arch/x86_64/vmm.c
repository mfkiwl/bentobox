#include "kernel/arch/x86_64/vmm.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/lfb.h>
#include <kernel/mmu.h>
#include <kernel/acpi.h>
#include <kernel/panic.h>
#include <kernel/sched.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>

uintptr_t initial_pml[3][512] __attribute__((aligned(PAGE_SIZE)));
uintptr_t *kernel_pd = initial_pml[0];

extern char text_start_ld[];
extern char text_end_ld[];
extern char rodata_start_ld[];
extern char rodata_end_ld[];
extern char data_start_ld[];
extern char data_end_ld[];
extern char bss_start_ld[];
extern char bss_end_ld[];

void vmm_flush_tlb(uintptr_t virt) {
    __asm__ volatile ("invlpg (%0)" ::"r"(virt) : "memory");
}

__attribute__((no_sanitize("undefined")))
void vmm_switch_pm(uintptr_t *pm) {
    if (pm == NULL)
        panic("Attempted to load a NULL pagemap!");
    asm volatile("mov %0, %%cr3" ::"r"((uint64_t)pm) : "memory");
    this_core()->pml4 = pm;
}

uintptr_t *vmm_get_next_lvl(uintptr_t *lvl, uintptr_t entry, uint64_t flags, bool alloc) {
    if (lvl[entry] & PTE_PRESENT) {
        return (uintptr_t *)PTE_GET_ADDR(lvl[entry]);
    }
    if (!alloc) {
        dprintf("%s:%d: \033[33mwarning:\033[0m couldn't get next pml\n", __FILE__, __LINE__);
        return NULL;
    }

    uintptr_t *pml = (uintptr_t *)mmu_alloc(1);
    memset(pml, 0, PAGE_SIZE);
    lvl[entry] = (uintptr_t)pml | flags;
    return pml;
}

void mmu_map_huge(uintptr_t virt, uintptr_t phys, uint64_t flags) {
    uintptr_t pml4_index = (virt >> 39) & 0x1ff;
    uintptr_t pdpt_index = (virt >> 30) & 0x1ff;
    uintptr_t pd_index = (virt >> 21) & 0x1ff;
 
    uintptr_t *pdpt = vmm_get_next_lvl(this_core()->pml4, pml4_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, true);
    uintptr_t *pd = vmm_get_next_lvl(pdpt, pdpt_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, true);
 
    pd[pd_index] = phys | flags | (1 << 7);
}

void mmu_unmap_huge(uintptr_t virt) {
    acquire(&this_core()->vmm_lock);

    uintptr_t pml4_index = (virt >> 39) & 0x1ff;
    uintptr_t pdpt_index = (virt >> 30) & 0x1ff;
    uintptr_t pd_index   = (virt >> 21) & 0x1ff;

    uint64_t *pml4 = this_core()->pml4;
    uint64_t *pdpt = vmm_get_next_lvl(pml4, pml4_index, 0, false);
    uint64_t *pd = vmm_get_next_lvl(pdpt, pdpt_index, 0, false);

    /* check if the page directory entry is present */
    if (pd[pd_index] & PTE_PRESENT) {
        /* clear the page directory entry */
        pd[pd_index] = 0;

        /* check if the page directory is empty */
        bool pd_empty = true;
        for (int i = 0; i < 512; i++) {
            if (pd[i] & PTE_PRESENT) {
                pd_empty = false;
                break;
            }
        }

        /* free it if it's empty */
        if (pd_empty) {
            mmu_free(pd, 1);
            pdpt[pdpt_index] = 0;
        }
    }

    /* check if the page directory pointer table entry is present */
    bool pdpt_empty = true;
    for (int i = 0; i < 512; i++) {
        if (pdpt[i] & PTE_PRESENT) {
            pdpt_empty = false;
            break;
        }
    }

    /* free it if it's empty */
    if (pdpt_empty) {
        mmu_free(pdpt, 1);
        pml4[pml4_index] = 0;
    }

    vmm_flush_tlb(virt);
    release(&this_core()->vmm_lock);
}

__attribute__((no_sanitize("undefined")))
void mmu_map(uintptr_t virt, uintptr_t phys, uint64_t flags) {
    acquire(&this_core()->vmm_lock);

    uintptr_t pml4_index = (virt >> 39) & 0x1ff;
    uintptr_t pdpt_index = (virt >> 30) & 0x1ff;
    uintptr_t pd_index = (virt >> 21) & 0x1ff;
    uintptr_t pt_index = (virt >> 12) & 0x1ff;
    
    uintptr_t *pdpt = vmm_get_next_lvl(this_core()->pml4, pml4_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, true);
    uintptr_t *pd = vmm_get_next_lvl(pdpt, pdpt_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, true);
    uintptr_t *pt = vmm_get_next_lvl(pd, pd_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, true);

    pt[pt_index] = phys | flags; /* map the page */
    
    vmm_flush_tlb(virt); /* flush the tlb entry */
    release(&this_core()->vmm_lock);
}

void mmu_unmap(uintptr_t virt) {
    acquire(&this_core()->vmm_lock);

    uintptr_t pml4_index = (virt >> 39) & 0x1ff;
    uintptr_t pdpt_index = (virt >> 30) & 0x1ff;
    uintptr_t pd_index   = (virt >> 21) & 0x1ff;
    uintptr_t pt_index   = (virt >> 12) & 0x1ff;

    uintptr_t *pml4 = this_core()->pml4;
    uintptr_t *pdpt = vmm_get_next_lvl(pml4, pml4_index, 0, false);
    uintptr_t *pd = vmm_get_next_lvl(pdpt, pdpt_index, 0, false);
    uintptr_t *pt = vmm_get_next_lvl(pd, pd_index, 0, false);

    pt[pt_index] = 0;

    /* check if the page table entry is present */
    bool pt_empty = true;
    for (int i = 0; i < 512; i++) {
        if (pt[i] & PTE_PRESENT) {
            pt_empty = false;
            break;
        }
    }

    /* free it if it's empty */
    if (pt_empty) {
        mmu_free(pt, 1);
        pd[pd_index] = 0;
    }

    /* check if the page directory entry is present */
    bool pd_empty = true;
    for (int i = 0; i < 512; i++) {
        if (pd[i] & PTE_PRESENT) {
            pd_empty = false;
            break;
        }
    }

    /* free it if it's empty */
    if (pd_empty) {
        mmu_free(pd, 1);
        pdpt[pdpt_index] = 0;
    }

    /* check if the page directory pointer table entry is present */
    bool pdpt_empty = true;
    for (int i = 0; i < 512; i++) {
        if (pdpt[i] & PTE_PRESENT) {
            pdpt_empty = false;
            break;
        }
    }

    /* free it if it's empty */
    if (pdpt_empty) {
        mmu_free(pdpt, 1);
        pml4[pml4_index] = 0;
    }

    vmm_flush_tlb(virt);
    release(&this_core()->vmm_lock);
}

void mmu_map_pages(uint32_t count, uintptr_t phys, uintptr_t virt, uint32_t flags) {
    for (uint32_t i = 0; i < count * PAGE_SIZE; i += PAGE_SIZE) {
        mmu_map(virt + i, phys + i, flags);
    }
}

void mmu_unmap_pages(uint32_t count, uintptr_t virt) {
    for (uint32_t i = 0; i < count * PAGE_SIZE; i += PAGE_SIZE) {
        mmu_unmap(virt + i);
    }
}

uintptr_t mmu_get_physical(uintptr_t *pml4, uintptr_t virt) {
    uintptr_t pml4_index = (virt >> 39) & 0x1ff;
    uintptr_t pdpt_index = (virt >> 30) & 0x1ff;
    uintptr_t pd_index = (virt >> 21) & 0x1ff;
    uintptr_t pt_index = (virt >> 12) & 0x1ff;
    
    uintptr_t *pdpt = vmm_get_next_lvl(pml4, pml4_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, false);
    uintptr_t *pd = vmm_get_next_lvl(pdpt, pdpt_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, false);
    uintptr_t *pt = vmm_get_next_lvl(pd, pd_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, false);

    return PTE_GET_ADDR(pt[pt_index]) | (virt & (PAGE_SIZE - 1));
}

uintptr_t *mmu_create_user_pm(struct task *proc) {
    uintptr_t *pml4 = (uintptr_t *)mmu_alloc(1);
    mmu_map((uintptr_t)pml4, (uintptr_t)pml4, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    this_core()->pml4 = pml4;
    pml4[511] = kernel_pd[511];

    //for (uintptr_t addr = 0x0; addr < 0x4000000; addr += 0x200000)
    //    mmu_map_huge(addr, addr, PTE_PRESENT | PTE_WRITABLE);
    mmu_map_huge(0x000000, 0x000000, PTE_PRESENT | PTE_WRITABLE);
    mmu_map_huge(0x200000, 0x200000, PTE_PRESENT | PTE_WRITABLE);

    return pml4;
}

void mmu_destroy_user_pm(uintptr_t *pml4) {
    this_core()->pml4 = pml4;

    //for (uintptr_t addr = 0x0; addr < 0x4000000; addr += 0x200000)
    //    mmu_unmap_huge(addr);
    mmu_unmap_huge(0x000000);
    mmu_unmap_huge(0x200000);
    // TODO: recursively unmap everything

    this_core()->pml4 = kernel_pd;
    mmu_unmap((uintptr_t)pml4);
    mmu_free(pml4, 1);
}

void vmm_install(void) {
    kernel_pd = (uintptr_t *)mmu_alloc(1);
    this_core()->pml4 = kernel_pd;
    memset(kernel_pd, 0, PAGE_SIZE);

    mmu_map_pages(511, 0x1000, 0x1000, PTE_PRESENT | PTE_USER);
    for (uintptr_t addr = 0x200000; addr < 0x4000000; addr += 0x200000)
        mmu_map_huge(addr, addr, PTE_PRESENT | PTE_WRITABLE);
    dprintf("%s:%d: done mapping kernel regions\n", __FILE__, __LINE__);

    vmm_switch_pm(kernel_pd);
    dprintf("%s:%d: successfully switched page tables\n", __FILE__, __LINE__);
}