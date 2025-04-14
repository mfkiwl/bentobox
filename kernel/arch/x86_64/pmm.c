#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <kernel/mmu.h>
#include <kernel/bitmap.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/spinlock.h>
#include <kernel/multiboot.h>

struct limine_hhdm_request hhdm = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0  
};

struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

uint8_t *pmm_bitmap = NULL;
uint64_t pmm_last_page = 0;
uint64_t mmu_page_count = 0;
uint64_t mmu_usable_mem = 0;
uint64_t mmu_used_pages = 0;

atomic_flag pmm_lock = ATOMIC_FLAG_INIT;

void pmm_install(void) {
    hhdm_offset = hhdm.response->offset;
    struct limine_memmap_response *memmap = memmap_request.response;
    struct limine_memmap_entry **entries = memmap->entries;
    struct limine_memmap_entry *first_usable_entry = NULL;    

    uintptr_t highest_address = 0;

    uint64_t i;
    for (i = 0; i < memmap->entry_count; i++) {
        if (entries[i]->type != LIMINE_MEMMAP_USABLE)
            continue;

        first_usable_entry = entries[i];
        highest_address = entries[i]->base + entries[i]->length;
    }

    pmm_bitmap = (uint8_t *)VIRTUAL(first_usable_entry->base);
    mmu_page_count = highest_address / PAGE_SIZE;
    uint64_t bitmap_size = ALIGN_UP(mmu_page_count / 8, PAGE_SIZE);
    first_usable_entry->base += bitmap_size;
    first_usable_entry->length -= bitmap_size;
    memset(pmm_bitmap, 0xFF, bitmap_size);

    for (i = 0; i < memmap->entry_count; i++) {
        if (entries[i]->type != LIMINE_MEMMAP_USABLE)
            continue;

        for (uint64_t j = 0; j < entries[i]->length; j += PAGE_SIZE) {
            bitmap_clear(pmm_bitmap, (entries[i]->base + j) / PAGE_SIZE);
        }
        mmu_usable_mem += entries[i]->length;
    }

    dprintf("%s:%d: initialized allocator at 0x%lx\n", __FILE__, __LINE__, (uint64_t)pmm_bitmap);
    dprintf("%s:%d: usable memory: %luK\n", __FILE__, __LINE__, mmu_usable_mem / 1024 - mmu_used_pages * 4);
}

void mmu_mark_used(void *ptr, size_t page_count) {
    for (size_t i = 0; i < page_count * PAGE_SIZE; i += PAGE_SIZE) {
        bitmap_set(pmm_bitmap, ((uintptr_t)ptr + i) / PAGE_SIZE);
    }
    mmu_used_pages += page_count;
}

// TODO: always search from the start of memory to avoid massive fragmentation

uint64_t pmm_find_pages(uint64_t page_count) {
    uint64_t pages = 0;
    uint64_t first_page = pmm_last_page;

    while (pages < page_count) {
        if (pmm_last_page >= mmu_page_count) {
            return 0; /* out of memory */
        }

        if (!bitmap_get(pmm_bitmap, pmm_last_page + pages)) {
            pages++;
            if (pages == page_count) {
                for (uint64_t i = 0; i < pages; i++) {
                    bitmap_set(pmm_bitmap, first_page + i);
                }

                pmm_last_page += pages;
                mmu_used_pages += pages;
                return first_page;
            }
        } else {
            pmm_last_page += !pages ? 1 : pages;
            first_page = pmm_last_page;
            pages = 0;
        }
    }
    return 0;
}

void *mmu_alloc(size_t page_count) {
    acquire(&pmm_lock);
    uint64_t pages = pmm_find_pages(page_count);
    
    if (!pages) {
        pmm_last_page = 0;
        pages = pmm_find_pages(page_count);

        if (!pages) {
            printf("%s:%d: allocation failed: out of memory\n", __FILE__, __LINE__);
            return NULL;
        }
    }

    uint64_t phys_addr = pages * PAGE_SIZE;
    
    release(&pmm_lock);
    return (void*)(phys_addr);
}

void mmu_free(void *ptr, size_t page_count) {
    acquire(&pmm_lock);
    uint64_t page = (uint64_t)ptr / PAGE_SIZE;

    for (uint64_t i = 0; i < page_count; i++)
        bitmap_clear(pmm_bitmap, page + i);
    mmu_used_pages -= page_count;
    release(&pmm_lock);
}