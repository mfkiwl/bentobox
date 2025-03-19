#include <multiboot.h>
#include <arch/x86_64/vmm.h>
#include <misc/string.h>
#include <misc/printf.h>
#include <misc/bitmap.h>
#include <stddef.h>
#include <stdint.h>

uint8_t *pmm_bitmap = NULL;
uint64_t pmm_last_page = 0;
uint64_t pmm_used_pages = 0;
uint64_t pmm_page_count = 0;
uint64_t pmm_usable_mem = 0;
uint64_t pmm_bitmap_size = 0;

void pmm_install(void *mboot_info) {
    extern void *end;
    uintptr_t highest_address = 0;

    struct multiboot_tag_mmap *mmap = mboot2_find_tag(mboot_info, MULTIBOOT_TAG_TYPE_MMAP);
    struct multiboot_mmap_entry *mmmt = NULL;

    for (uint32_t i = 0; i < (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size; i++) {
        mmmt = &mmap->entries[i];

        if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE) {
            dprintf("%s:%d: Memory segment: addr=0x%x, len=0x%x, type=%u\n",
                __FILE__, __LINE__, mmmt->addr, mmmt->len, mmmt->type);
            highest_address = mmmt->addr + mmmt->len;
        }
    }

    pmm_bitmap = (uint8_t *)&end;
    pmm_page_count = highest_address / PAGE_SIZE;
    uint64_t bitmap_size = ALIGN_UP(pmm_page_count / 8, PAGE_SIZE);
    memset(pmm_bitmap, bitmap_size, 0xFF);

    for (uint32_t i = 0; i < (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size; i++) {
        mmmt = &mmap->entries[i];

        if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE) {
            for (uint64_t o = 0; o < mmmt->len; o += PAGE_SIZE) {
                bitmap_clear(pmm_bitmap, (mmmt->addr + i) / PAGE_SIZE);
            }
        }
    }

    dprintf("%s:%d: initialized allocator at 0x%x\n", __FILE__, __LINE__, (uint64_t)pmm_bitmap);
    dprintf("%s:%d: usable memory: %dK\n", __FILE__, __LINE__, pmm_page_count * 4);
}

uint64_t pmm_find_pages(uint64_t page_count) {
    uint64_t pages = 0;
    uint64_t first_page = pmm_last_page;

    while (pages < page_count) {
        if (pmm_last_page >= pmm_page_count) {
            return 0; /* out of memory */
        }

        if (!bitmap_get(pmm_bitmap, pmm_last_page + pages)) {
            pages++;
            if (pages == page_count) {
                for (uint64_t i = 0; i < pages; i++) {
                    bitmap_set(pmm_bitmap, first_page + i);
                }

                pmm_last_page += pages;
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

void *pmm_alloc(size_t page_count) {
    uint64_t pages = pmm_find_pages(page_count);
    
    if (!pages) {
        pmm_last_page = 0;
        pages = pmm_find_pages(page_count);

        if (!pages) {
            dprintf("%s:%d: \033[33mwarning:\033[0m out of memory\n", __FILE__, __LINE__);
            return NULL;
        }
    }

    uint64_t phys_addr = pages * PAGE_SIZE;

    return (void*)(phys_addr);
}

void pmm_free(void *ptr, size_t page_count) {
    uint64_t page = (uint64_t)ptr / PAGE_SIZE;

    for (uint64_t i = 0; i < page_count; i++)
        bitmap_clear(pmm_bitmap, page + i);
}