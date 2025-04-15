#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/lfb.h>
#include <kernel/mmu.h>
#include <kernel/panic.h>
#include <kernel/sched.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>

struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0
};

uint64_t hhdm_offset = 0;
uintptr_t *kernel_pd = NULL;

extern char text_start_ld[];
extern char text_end_ld[];
extern char rodata_start_ld[];
extern char rodata_end_ld[];
extern char data_start_ld[];
extern char data_end_ld[];

void vmm_flush_tlb(uintptr_t virt) {
    __asm__ volatile ("invlpg (%0)" ::"r"(virt) : "memory");
}

__attribute__((no_sanitize("undefined")))
void vmm_switch_pm(uintptr_t *pm) {
    if (pm == NULL)
        panic("Attempted to load a NULL pagemap!");
    asm volatile("mov %0, %%cr3" ::"r"((uint64_t)PHYSICAL(pm)) : "memory");
    this_core()->pml4 = pm;
}

/*
 * vmm_get_next_lvl - gets the next level page table
 */
__attribute__((no_sanitize("undefined")))
uintptr_t *vmm_get_next_lvl(uintptr_t *lvl, uintptr_t entry, uint64_t flags, bool alloc) {
    if (lvl[entry] & PTE_PRESENT) {
        return VIRTUAL(PTE_GET_ADDR(lvl[entry]));
    }
    if (!alloc) {
        dprintf("%s:%d: \033[33mwarning:\033[0m couldn't get next pml\n", __FILE__, __LINE__);
        return NULL;
    }

    uintptr_t *pml = (uintptr_t*)VIRTUAL(mmu_alloc(1));
    memset(pml, 0, PAGE_SIZE);
    lvl[entry] = (uintptr_t)PHYSICAL(pml) | flags;
    return pml;
}

void mmu_map(uintptr_t virt, uintptr_t phys, uint64_t flags) {
    acquire(&this_core()->vmm_lock);

    struct cpu *this = this_core();
    uintptr_t pml4_index = (virt >> 39) & 0x1ff;
    uintptr_t pdpt_index = (virt >> 30) & 0x1ff;
    uintptr_t pd_index = (virt >> 21) & 0x1ff;
    uintptr_t pt_index = (virt >> 12) & 0x1ff;

    /* TODO: use separate pagemaps for usermode processes */
    
    uintptr_t *pdpt = vmm_get_next_lvl(this->pml4, pml4_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, true);
    uintptr_t *pd = vmm_get_next_lvl(pdpt, pdpt_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, true);
    uintptr_t *pt = vmm_get_next_lvl(pd, pd_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, true);

    pt[pt_index] = phys | flags; /* map the page */
    
    vmm_flush_tlb(virt); /* flush the tlb entry */
    release(&this_core()->vmm_lock);
}

void mmu_unmap(uintptr_t virt) {
    acquire(&this_core()->vmm_lock);

    struct cpu *this = this_core();
    uintptr_t pml4_index = (virt >> 39) & 0x1ff;
    uintptr_t pdpt_index = (virt >> 30) & 0x1ff;
    uintptr_t pd_index = (virt >> 21) & 0x1ff;
    uintptr_t pt_index = (virt >> 12) & 0x1ff;

    uint64_t *pdpt = vmm_get_next_lvl(this->pml4, pml4_index, 0, false);
    uint64_t *pd = vmm_get_next_lvl(pdpt, pdpt_index, 0, false);
    uint64_t *pt = vmm_get_next_lvl(pd, pd_index, 0, false);

    /* clear the page table entry */
    pt[pt_index] = 0; /* unmap the page */

    /* check if the page table is empty and free it */
    bool empty = true;
    for (int i = 0; i < 512; i++) {
        if (pt[i] & PTE_PRESENT) { /* check if any entry is present */
            empty = false;
            break;
        }
    }

    if (empty) {
        mmu_free(pt, 1); /* free the page table if it is empty */
        pd[pd_index] = 0x00000000; /* clear the pd entry */
    }

    vmm_flush_tlb(virt); /* flush the tlb entry */
    release(&this_core()->vmm_lock);
}

void mmu_map_huge(uintptr_t virt, uintptr_t phys, uint64_t flags) {
    struct cpu *this = this_core();
    uintptr_t pml4_index = (virt >> 39) & 0x1ff;
    uintptr_t pdpt_index = (virt >> 30) & 0x1ff;
    uintptr_t pd_index = (virt >> 21) & 0x1ff;

    uintptr_t *pdpt = vmm_get_next_lvl(this->pml4, pml4_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, true);
    uintptr_t *pd = vmm_get_next_lvl(pdpt, pdpt_index, PTE_PRESENT | PTE_WRITABLE | PTE_USER, true);

    pd[pd_index] = phys | flags | (1 << 7);
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

void vmm_map_kernel(void) {
    uintptr_t phys_base = kernel_address_request.response->physical_base;
    uintptr_t virt_base = kernel_address_request.response->virtual_base;

    uintptr_t text_start = ALIGN_DOWN((uintptr_t)text_start_ld, PAGE_SIZE);
    uintptr_t text_end = ALIGN_UP((uintptr_t)text_end_ld, PAGE_SIZE);
    uintptr_t rodata_start = ALIGN_DOWN((uintptr_t)rodata_start_ld, PAGE_SIZE);
    uintptr_t rodata_end = ALIGN_UP((uintptr_t)rodata_end_ld, PAGE_SIZE);
    uintptr_t data_start = ALIGN_DOWN((uintptr_t)data_start_ld, PAGE_SIZE);
    uintptr_t data_end = ALIGN_UP((uintptr_t)data_end_ld, PAGE_SIZE);

    for (uintptr_t text = text_start; text < text_end; text += PAGE_SIZE)
        mmu_map(text, text - virt_base + phys_base, PTE_PRESENT);
    for (uintptr_t rodata = rodata_start; rodata < rodata_end; rodata += PAGE_SIZE)
        mmu_map(rodata, rodata - virt_base + phys_base, PTE_PRESENT | PTE_NX);
    for (uintptr_t data = data_start; data < data_end; data += PAGE_SIZE)
        mmu_map(data, data - virt_base + phys_base, PTE_PRESENT | PTE_WRITABLE | PTE_NX);
    for (uintptr_t addr = 0; addr < 0x100000000; addr += 0x200000)
        mmu_map_huge((uintptr_t)VIRTUAL(addr), addr, PTE_PRESENT | PTE_WRITABLE);
}

void mmu_create_user_pm(uintptr_t *pml4) {
    this_core()->pml4 = pml4;

    vmm_map_kernel();
    mmu_map((uintptr_t)madt_ioapic_list[0]->address, (uintptr_t)madt_ioapic_list[0]->address, PTE_PRESENT | PTE_WRITABLE);

    mmu_map((uintptr_t)VIRTUAL(LAPIC_REGS), LAPIC_REGS, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    mmu_map((uintptr_t)VIRTUAL(hpet->address), hpet->address, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    mmu_map((uintptr_t)ALIGN_DOWN((uintptr_t)hpet, PAGE_SIZE), (uintptr_t)ALIGN_DOWN((uintptr_t)hpet, PAGE_SIZE), PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    mmu_map_pages((ALIGN_UP((lfb.pitch * lfb.height), PAGE_SIZE) / PAGE_SIZE), (uintptr_t)lfb.addr, (uintptr_t)lfb.addr, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
}

void vmm_install(void) {
    kernel_pd = (uintptr_t *)VIRTUAL(mmu_alloc(1));
    memset(kernel_pd, 0, PAGE_SIZE);
    this_core()->pml4 = kernel_pd;

    vmm_map_kernel();
    dprintf("%s:%d: done mapping kernel regions\n", __FILE__, __LINE__);

    vmm_switch_pm(kernel_pd);
    dprintf("%s:%d: successfully switched page tables\n", __FILE__, __LINE__);
}