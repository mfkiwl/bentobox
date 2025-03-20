#pragma once
#include <stddef.h>

void  pmm_install(void *mboot_info);
void *pmm_alloc(size_t page_count);
void  pmm_free(void *ptr, size_t page_count);