#include <stdbool.h>
#include <arch/x86_64/pmm.h>
#include <arch/x86_64/vmm.h>
#include <misc/heap.h>
#include <misc/printf.h>

struct heap *kernel_heap;

void *kmalloc(size_t n) {
    return heap_alloc(kernel_heap, n);
}

void kfree(void *ptr) {
    heap_free(ptr);
}

__attribute__((no_sanitize("undefined")))
struct heap *heap_create(void) {
    struct heap *h = (struct heap *)VIRTUAL(pmm_alloc(1));
    vmm_map((uintptr_t)h, (uintptr_t)PHYSICAL(h), PTE_PRESENT | PTE_WRITABLE);
    h->head = (struct heap_block *)VIRTUAL(pmm_alloc(1));
    vmm_map((uintptr_t)h->head, (uintptr_t)PHYSICAL(h), PTE_PRESENT | PTE_WRITABLE);
    h->head->next = h->head;
    h->head->prev = h->head;
    h->head->size = 0;
    h->head->magic = HEAP_MAGIC;

    dprintf("%s:%d: created heap at 0x%x\n", __FILE__, __LINE__, (uint64_t)h);
    return h;
}

__attribute__((no_sanitize("undefined")))
void heap_delete(struct heap *h) {
    struct heap_block *current = h->head->next;
    struct heap_block *next;

    while (current != h->head) {
        next = current->next;
        uint32_t pages = DIV_CEILING(sizeof(struct heap_block) + current->size, PAGE_SIZE);
        pmm_free(PHYSICAL(current), pages);
        vmm_unmap_pages(pages, (uintptr_t)current);
        current = next;
    }

    pmm_free(PHYSICAL(h->head), 1);
    vmm_unmap((uintptr_t)h->head);
    pmm_free(PHYSICAL(h), 1);
    vmm_unmap((uintptr_t)h);
}

__attribute__((no_sanitize("undefined")))
void *heap_alloc(struct heap *h, uint64_t n) {
    uint64_t pages = DIV_CEILING(sizeof(struct heap_block) + n, PAGE_SIZE);
    
    struct heap_block *block = (struct heap_block *)VIRTUAL(pmm_alloc(pages));
    vmm_map((uintptr_t)block, (uintptr_t)PHYSICAL(block), PTE_PRESENT | PTE_WRITABLE);
    block->next = h->head;
    block->prev = h->head->prev;
    block->size = n;
    block->magic = HEAP_MAGIC;

    return (void*)block + sizeof(struct heap_block);
}

__attribute__((no_sanitize("undefined")))
void heap_free(void *ptr) {
    struct heap_block *block = (struct heap_block *)(ptr - sizeof(struct heap_block));

    if (block->magic != HEAP_MAGIC) {
        printf("%s:%d: bad block magic at address 0x%x\n", __FILE__, __LINE__, (uint64_t)block);
        return;
    }

    block->prev->next = block->next;
    block->next->prev = block->prev;
    uint64_t pages = DIV_CEILING(sizeof(struct heap_block) + block->size, PAGE_SIZE);

    pmm_free(PHYSICAL(block), pages);
    vmm_unmap((uintptr_t)block);
}