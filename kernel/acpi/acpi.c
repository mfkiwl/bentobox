#include <stdint.h>
#include <stdbool.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/pmm.h>
#include <arch/x86_64/vmm.h>
#include <acpi/acpi.h>
#include <acpi/fadt.h>
#include <acpi/madt.h>
#include <misc/string.h>
#include <misc/printf.h>
#include <misc/assert.h>

bool acpi_use_xsdt = false;
void *acpi_root_sdt;

__attribute__((no_sanitize("alignment")))
void *acpi_find_table(const char *signature) {
    if (!acpi_use_xsdt) {
        struct acpi_rsdt *rsdt = (struct acpi_rsdt*)acpi_root_sdt;
        uint32_t entries = (rsdt->sdt.length - sizeof(rsdt->sdt)) / 4;

        for (uint32_t i = 0; i < entries; i++) {
            struct acpi_sdt *sdt = (struct acpi_sdt*)(uintptr_t)(*((uint32_t*)rsdt->table + i));
            if (!memcmp(sdt->signature, signature, 4)) {
                return (void*)sdt;
            }
        }

        dprintf("%s:%d: ACPI: couldn't find table %s\n", __FILE__, __LINE__, signature);
        return NULL;
    }
    
    /* use xsdt */
    struct acpi_xsdt *rsdt = (struct acpi_xsdt*)acpi_root_sdt;
    uint32_t entries = (rsdt->sdt.length - sizeof(rsdt->sdt)) / 8;
        
    for (uint32_t i = 0; i < entries; i++) {
        struct acpi_sdt *sdt = (struct acpi_sdt*)(uintptr_t)(*((uint32_t*)rsdt->table + i));
        if (!memcmp(sdt->signature, signature, 4)) {
            return (void*)sdt;
        }
    }

    dprintf("%s:%d: ACPI: couldn't find table %s\n", __FILE__, __LINE__, signature);
    return NULL;
}

__attribute__((no_sanitize("undefined")))
void acpi_install(void) {
    struct acpi_rsdp *rsdp = NULL;

    /* TODO: search for RSDP in multiboot2 headers */
    for (uint16_t *addr = (uint16_t*)0x000E0000; addr < (uint16_t*)0x000FFFFF; addr += 16) {
        if (!strncmp((const char*)addr, "RSD PTR ", 8)) {
            rsdp = (struct acpi_rsdp *)addr;
            dprintf("%s:%d: found RSDP at address 0x%x\n", __FILE__, __LINE__, addr);
            break;
        }
    }

    /* TODO: use panic() */
    assert(rsdp);

    if (rsdp->revision != 0) {
        /* use xsdt */
        acpi_use_xsdt = true;
        struct acpi_xsdp *xsdp = (struct acpi_xsdp*)rsdp;
        acpi_root_sdt = (struct acpi_xsdt*)(xsdp->xsdt_addr);
    } else {
        acpi_root_sdt = (struct acpi_xsdt*)(uintptr_t)(rsdp->rsdt_addr);
    }
    dprintf("%s:%d: ACPI version %s\n", __FILE__, __LINE__, acpi_use_xsdt ? "2.0" : "1.0");

    vmm_map((uintptr_t)acpi_root_sdt, (uintptr_t)acpi_root_sdt, PTE_PRESENT);
    fadt_init();
    madt_init();

    dprintf("%s:%d: initialized ACPI tables\n", __FILE__, __LINE__);
}

void acpi_reboot(void) {
#ifdef __x86_64__
    outb(fadt->reset_reg.address, fadt->reset_val);
#else
    printf("%s:%d (%s) failed to reboot: not implemented\n", __FILE__, __LINE__, __func__);
#endif
}