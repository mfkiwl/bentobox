#include <stdbool.h>
#include <kernel/mmu.h>
#include <kernel/malloc.h>
#include <kernel/printf.h>

#define HEAP_MAGIC 0x58524332

struct heap *kernel_heap;

void *kmalloc(size_t n) {
    return heap_alloc(kernel_heap, n);
}

void kfree(void *ptr) {
    heap_free(ptr);
}

void create_kernel_heap(void) {
    kernel_heap = heap_create();
}

__attribute__((no_sanitize("undefined")))
struct heap *heap_create(void) {
    struct heap *h = (struct heap *)VIRTUAL(mmu_alloc(1));
    mmu_map(h, PHYSICAL(h), PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    h->head = (struct heap_block *)VIRTUAL(mmu_alloc(1));
    mmu_map(h->head, PHYSICAL(h->head), PTE_PRESENT | PTE_WRITABLE | PTE_USER); // TODO: does this need fixing?
    h->head->next = h->head;
    h->head->prev = h->head;
    h->head->size = 0;
    h->head->magic = HEAP_MAGIC;
    return h;
}

__attribute__((no_sanitize("undefined")))
void heap_delete(struct heap *h) {
    struct heap_block *current = h->head->next;
    struct heap_block *next;

    while (current != h->head) {
        next = current->next;
        size_t pages = DIV_CEILING(sizeof(struct heap_block) + current->size, PAGE_SIZE);
        mmu_free(PHYSICAL(current), pages);
        mmu_unmap_pages(pages, (void *)current);
        current = next;
    }

    mmu_free(PHYSICAL(h->head), 1);
    mmu_unmap(h->head);
    mmu_free(PHYSICAL(h), 1);
    mmu_unmap(h);
}

__attribute__((no_sanitize("undefined")))
void *heap_alloc(struct heap *h, uint64_t n) {
    if (n == 0) {
        printf("%s:%d: \033[33mwarning:\033[0m allocating 0 bytes\n", __FILE__, __LINE__);
    }

    uint64_t pages = DIV_CEILING(sizeof(struct heap_block) + n, PAGE_SIZE);
    
    struct heap_block *block = (struct heap_block *)VIRTUAL(mmu_alloc(pages));
    if (!block) {
        printf("%s:%d: allocation failed\n", __FILE__, __LINE__);
        return NULL;
    }
    mmu_map_pages(pages, block, PHYSICAL(block), PTE_PRESENT | PTE_WRITABLE | PTE_USER);
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

    mmu_free(PHYSICAL(block), pages);
    mmu_unmap_pages(pages, block);
}