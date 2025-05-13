#include <stdbool.h>
#include <kernel/mmu.h>
#include <kernel/vma.h>
#include <kernel/printf.h>

struct vma_head *vma_create(void) {
    struct vma_head *h = (struct vma_head *)VIRTUAL(mmu_alloc(1));
    mmu_map((uintptr_t)h, (uintptr_t)PHYSICAL(h), PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    h->head = (struct vma_block *)VIRTUAL(mmu_alloc(1));
    mmu_map((uintptr_t)h->head, (uintptr_t)PHYSICAL(h->head), PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    h->head->next = h->head;
    h->head->prev = h->head;
    h->head->size = 0;
    h->head->checksum = 0;
    h->head->phys = 0;
    h->head->virt = 0;
    return h;
}

void vma_destroy(struct vma_head *h) {
    struct vma_block *current = h->head->next;
    struct vma_block *next;

    while (current != h->head) {
        next = current->next;
        mmu_unmap_pages(current->size, current->virt);
        mmu_free((void *)current->phys, current->size);
        current = next;
    }

    mmu_free(PHYSICAL(h->head), 1);
    mmu_unmap((uintptr_t)h->head);
    mmu_free(PHYSICAL(h), 1);
    mmu_unmap((uintptr_t)h);
}

void *vma_map(struct vma_head *h, uint64_t pages, uint64_t phys, uint64_t virt, uint64_t flags) {
    struct vma_block *block = VIRTUAL(mmu_alloc(1));
    mmu_map((uintptr_t)block, (uintptr_t)PHYSICAL(block), PTE_PRESENT | PTE_WRITABLE);
    block->next = h->head;
    block->prev = h->head->prev;
    h->head->prev->next = block;
    h->head->prev = block;
    block->size = pages;
    if (!phys && !virt) {
        block->phys = (uintptr_t)mmu_alloc(pages);
        block->virt = (uintptr_t)VIRTUAL(block->phys);
    } else {
        block->phys = phys;
        block->virt = virt;
    }
    mmu_map_pages(pages, block->phys, block->virt, flags);

    block->checksum = block->phys + block->virt;

    return (void *)block->virt;
}

void vma_unmap(struct vma_block *block) {
    if (block->phys + block->virt != block->checksum) {
        printf("%s:%d: bad checksum at address 0x%x\n", __FILE__, __LINE__, (uint64_t)block);
        return;
    }

    block->prev->next = block->next;
    block->next->prev = block->prev;

    mmu_unmap_pages(block->size, block->virt);
    mmu_free((void *)block->phys, block->size);
    mmu_unmap((uintptr_t)block);
    mmu_free(PHYSICAL(block), 1);
}