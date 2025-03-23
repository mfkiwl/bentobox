#include <stdint.h>
#include <kernel/printf.h>

struct gdt_entry {
    uint16_t limit;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
} __attribute__((packed));

struct gdtr {
    uint16_t size;
    uint64_t offset;
} __attribute__((packed));

struct gdt_entry gdt_entries[9];
struct gdtr gdt_descriptor;

extern void gdt_flush(void);

void gdt_set_entry(uint8_t index, uint16_t limit, uint32_t base, uint8_t access, uint8_t gran) {
    gdt_entries[index].limit = limit;
    gdt_entries[index].base_low = base & 0xFFFF;
    gdt_entries[index].base_mid = (base >> 16) & 0xFF;
    gdt_entries[index].access = access;
    gdt_entries[index].gran = gran;
    gdt_entries[index].base_high = (base >> 24) & 0xFF;
}

void gdt_install(void) {
    gdt_set_entry(0, 0x0000, 0x00000000, 0x00, 0x00);
    gdt_set_entry(1, 0x0000, 0x00000000, 0x9A, 0x20);
    gdt_set_entry(2, 0x0000, 0x00000000, 0x92, 0x00);
    gdt_set_entry(3, 0x0000, 0x00000000, 0xFA, 0x20);
    gdt_set_entry(4, 0x0000, 0x00000000, 0xF2, 0x00);

    gdt_descriptor = (struct gdtr) {
        .size = sizeof(struct gdt_entry) * 9 - 1,
        .offset = (uint64_t)&gdt_entries
    };
    gdt_flush();

    dprintf("%s:%d: GDT address: 0x%lx\n", __FILE__, __LINE__, (uint64_t)&gdt_descriptor);
}