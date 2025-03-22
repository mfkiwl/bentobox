#include <stdint.h>
#include <stddef.h>

void *mmu_alloc(size_t page_count) { return NULL; }
void  mmu_free(void *ptr, size_t page_count) {}
void  mmu_map(uintptr_t virt, uintptr_t phys, uint64_t flags) {}
void  mmu_unmap(uintptr_t virt) {}
void  mmu_map_pages(uint32_t count, uintptr_t phys, uintptr_t virt, uint32_t flags) {}
void  mmu_unmap_pages(uint32_t count, uintptr_t virt) {}