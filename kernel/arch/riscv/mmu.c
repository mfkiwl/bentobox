#include <stdint.h>
#include <stddef.h>
#include <kernel/assert.h>

void *mmu_alloc(size_t page_count) { unimplemented; return NULL; }
void  mmu_free(void *ptr, size_t page_count) { unimplemented; }
void  mmu_map(uintptr_t virt, uintptr_t phys, uint64_t flags) { unimplemented; }
void  mmu_unmap(uintptr_t virt) { unimplemented; }
void  mmu_map_pages(uint32_t count, uintptr_t phys, uintptr_t virt, uint32_t flags) { unimplemented; }
void  mmu_unmap_pages(uint32_t count, uintptr_t virt) { unimplemented; }