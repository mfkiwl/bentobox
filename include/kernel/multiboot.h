#pragma once
#include <stdint.h>

#define MULTIBOOT_TAG_TYPE_MMAP           6

#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
}__attribute__((packed));

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[];  
}__attribute__((packed));

void *mboot2_find_tag(void *base, uint32_t type);