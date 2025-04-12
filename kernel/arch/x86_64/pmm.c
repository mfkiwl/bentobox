#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <kernel/arch/x86_64/vmm.h>
#include <kernel/mmu.h>
#include <kernel/bitmap.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/spinlock.h>
#include <kernel/multiboot.h>

uint8_t *pmm_bitmap = NULL;
uint64_t pmm_last_page = 0;
uint64_t mmu_page_count = 0;
uint64_t mmu_usable_mem = 0;
uint64_t mmu_used_pages = 0;

atomic_flag pmm_lock = ATOMIC_FLAG_INIT;

void pmm_install(void *mboot_info) {
    extern void *end;
    uintptr_t highest_address = 0;

    struct multiboot_tag_mmap *mmap = mboot2_find_tag(mboot_info, MULTIBOOT_TAG_TYPE_MMAP);
    struct multiboot_mmap_entry *mmmt = NULL;

    uint32_t i;
    for (i = 0; i < (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size; i++) {
        mmmt = &mmap->entries[i];
        
        if (mmmt->addr < KERNEL_PHYS_BASE) {
            mmmt->type = MULTIBOOT_MEMORY_RESERVED;
            continue;
        }

        if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE) {
            if (mmmt->addr >= KERNEL_PHYS_BASE && mmmt->addr < (uintptr_t)&end) {
                mmmt->len -= (uintptr_t)&end - KERNEL_PHYS_BASE;
                mmmt->addr = (uintptr_t)&end;
            }
            highest_address = mmmt->addr + mmmt->len;
        }
    }

    pmm_bitmap = (uint8_t *)PAGE_SIZE;
    mmu_page_count = highest_address / PAGE_SIZE;
    uint64_t bitmap_size = ALIGN_UP(mmu_page_count / 8, PAGE_SIZE);
    memset(pmm_bitmap, 0xFF, bitmap_size);

    for (i = 0; i < (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size; i++) {
        mmmt = &mmap->entries[i];

        if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE) {
            for (uint64_t j = 0; j < mmmt->len; j += PAGE_SIZE) {
                bitmap_clear(pmm_bitmap, (mmmt->addr + j) / PAGE_SIZE);
            }
            mmu_usable_mem += mmmt->len;
        }
    }

    struct multiboot_tag_module *mod = mboot2_find_tag(mboot_info, MULTIBOOT_TAG_TYPE_MODULE);
    while (mod) {
        mmu_mark_used((void *)(uintptr_t)mod->mod_start, ALIGN_UP(mod->mod_end - mod->mod_start, PAGE_SIZE) / PAGE_SIZE);
        mod = mboot2_find_next((char *)mod + ALIGN_UP(mod->size, 8), MULTIBOOT_TAG_TYPE_MODULE);
    }

	mmu_mark_used(mboot_info, 2);

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