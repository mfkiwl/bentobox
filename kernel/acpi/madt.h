#pragma once
#include <stdint.h>

struct acpi_madt {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;

    /* madt */
    uint32_t lapic_addr;
    uint32_t flags;

    char table[];
} __attribute__((packed));

struct madt_entry {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct madt_ioapic {
    struct madt_entry entry;
    uint8_t id;
    uint8_t resv;
    uint32_t address;
    uint32_t gsi_base;
} __attribute__((packed));

struct madt_iso {
    struct madt_entry entry;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));

struct madt_lapic_addr {
    struct madt_entry entry;
    uint16_t reserved;
    uint64_t lapic_address;
} __attribute__((packed));

extern struct acpi_madt *madt;
extern struct madt_ioapic *madt_ioapic_list[16];
extern struct madt_iso *madt_iso_list[16];
extern uint32_t madt_lapics;
extern uint32_t madt_ioapics;
extern uint32_t madt_isos;

extern struct madt_lapic_addr *lapic_addr;

void madt_init();