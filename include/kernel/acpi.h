#pragma once
#include <stdint.h>
#include <stdbool.h>

struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;
} __attribute__((packed));

struct acpi_xsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;

    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct acpi_sdt {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct acpi_rsdt {
    struct acpi_sdt sdt;
    char table[];
} __attribute__((packed));

struct acpi_xsdt {
    struct acpi_sdt sdt;
    char table[];
} __attribute__((packed));

struct acpi_gas {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} __attribute__((packed));

struct acpi_fadt {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;

    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t resv;
    uint8_t preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_cnt;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t pm1_evt_len;
    uint8_t pm1_cnt_len;
    uint8_t pm2_cnt_len;
    uint8_t pm_tmr_len;
    uint8_t gpe0_blk_len;
    uint8_t gpe1_blk_len;
    uint8_t gpe1_base;
    uint8_t cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alrm;
    uint8_t mon_alrm;
    uint8_t century;
    uint16_t iapc_boot_arch;
    uint8_t resv2;
    uint32_t flags;
    struct acpi_gas reset_reg;
    uint8_t reset_val;
    uint16_t arm_boot_arch;
    uint8_t fadt_ver_minor;
    uint64_t x_firmware_ctrl;
    uint64_t x_dsdt;
    struct acpi_gas x_pm1a_evt_blk;
    struct acpi_gas x_pm1b_evt_blk;
    struct acpi_gas x_pm1a_cnt_blk;
    struct acpi_gas x_pm1b_cnt_blk;
    struct acpi_gas x_pm2_cnt_blk;
    struct acpi_gas x_pm_tmr_blk;
    struct acpi_gas x_gpe0_blk;
    struct acpi_gas x_gpe1_blk;
    struct acpi_gas sleep_ctrl_reg;
    struct acpi_gas sleep_status;
    uint64_t hypervisor_vendor_id;
} __attribute__((packed));

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

extern bool acpi_use_xsdt;
extern void *acpi_root_sdt;

extern struct acpi_fadt *fadt;
extern struct acpi_madt *madt;
extern struct madt_ioapic *madt_ioapic_list[16];
extern struct madt_iso *madt_iso_list[16];
extern struct madt_lapic_addr *lapic_addr;
extern uint32_t madt_lapics;
extern uint32_t madt_ioapics;
extern uint32_t madt_isos;

void acpi_install(void);
void acpi_reboot(void);
void *acpi_find_table(const char *signature);

void fadt_init(void);
void madt_init(void);