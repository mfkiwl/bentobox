#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <kernel/arch/x86_64/vmm.h>
#include <kernel/mmu.h>
#include <kernel/panic.h>
#include <kernel/bitmap.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/spinlock.h>
#include <kernel/multiboot.h>

uint8_t *pmm_bitmap = NULL;
uint64_t pmm_bitmap_size = 0;
uint64_t mmu_page_count = 0;
uint64_t mmu_usable_mem = 0;
uint64_t mmu_used_pages = 0;

atomic_flag pmm_lock = ATOMIC_FLAG_INIT;

void pmm_install(void) {
    extern void *mboot, end;
    uintptr_t highest_address = 0;

    struct multiboot_tag_mmap *mmap = mboot2_find_tag(mboot, MULTIBOOT_TAG_TYPE_MMAP);
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
    pmm_bitmap_size = ALIGN_UP(mmu_page_count / 8, PAGE_SIZE);
    memset(pmm_bitmap, 0xFF, pmm_bitmap_size);

    for (i = 0; i < (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size; i++) {
        mmmt = &mmap->entries[i];

        if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE) {
            for (uint64_t j = 0; j < mmmt->len; j += PAGE_SIZE) {
                bitmap_clear(pmm_bitmap, (mmmt->addr + j) / PAGE_SIZE);
            }
            mmu_usable_mem += mmmt->len;
        }
    }

    struct multiboot_tag_module *mod = mboot2_find_tag(mboot, MULTIBOOT_TAG_TYPE_MODULE);
    while (mod) {
        mmu_mark_used((void *)(uintptr_t)mod->mod_start, ALIGN_UP(mod->mod_end - mod->mod_start, PAGE_SIZE) / PAGE_SIZE);
        mod = mboot2_find_next((char *)mod + ALIGN_UP(mod->size, 8), MULTIBOOT_TAG_TYPE_MODULE);
    }

	mmu_mark_used(mboot, 2);
    
    dprintf("%s:%d: initialized bitmap at 0x%p\n", __FILE__, __LINE__, (uint64_t)pmm_bitmap);
    dprintf("%s:%d: usable memory: %luK\n", __FILE__, __LINE__, mmu_usable_mem / 1024 - mmu_used_pages * 4);
}

void mmu_mark_used(void *ptr, size_t page_count) {
    for (size_t i = 0; i < page_count * PAGE_SIZE; i += PAGE_SIZE) {
        bitmap_set(pmm_bitmap, ((uintptr_t)ptr + i) / PAGE_SIZE);
    }
    mmu_used_pages += page_count;
}

uint64_t pmm_find_pages(uint64_t page_count) {
    uint64_t pages = 0;
    uint64_t first_page = 0;

    for (uint64_t i = 0; i < mmu_page_count; i++) {
        if (!bitmap_get(pmm_bitmap, i)) {
            if (pages == 0) {
                first_page = i;
            }
            pages++;
            if (pages == page_count) {
                for (uint64_t j = 0; j < page_count; j++) {
                    acquire(&pmm_lock);
                    bitmap_set(pmm_bitmap, first_page + j);
                    release(&pmm_lock);
                }

                mmu_used_pages += page_count;
                return first_page;
            }
        } else {
            pages = 0;
        }
    }
    return 0;
}

void *mmu_alloc(size_t page_count) {
    uint64_t pages = pmm_find_pages(page_count);
    
    if (!pages)
        panic("allocation failed: out of memory");

    uint64_t phys_addr = pages * PAGE_SIZE;
    
    return (void*)(phys_addr);
}

void mmu_free(void *ptr, size_t page_count) {
    uint64_t page = (uint64_t)ptr / PAGE_SIZE;

    if ((uintptr_t)ptr == 0x52b000) {
        //panic("breakpoint");
    }

    if ((uintptr_t)ptr < KERNEL_PHYS_BASE || page > pmm_bitmap_size * 8) {
        panic("invalid deallocation @ 0x%p", ptr);
        printf("%s:%d: invalid deallocation @ 0x%p\n", __FILE__, __LINE__, ptr);
        return;
    }

    acquire(&pmm_lock);
    for (uint64_t i = 0; i < page_count; i++) {
        if (!bitmap_get(pmm_bitmap, page + i)) {
            panic("double free @ 0x%p", ptr);
            //dprintf("%s:%d: double free @ 0x%p\n", __FILE__, __LINE__, ptr);
            return;
        }
        bitmap_clear(pmm_bitmap, page + i);
    }
    release(&pmm_lock);
    
    mmu_used_pages -= page_count;
}