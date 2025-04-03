#pragma once
#include <stdint.h>

void gdt_install(void);
void gdt_set_entry(uint8_t index, uint16_t limit, uint32_t base, uint8_t access, uint8_t gran);
extern void gdt_flush(void);