#include <stddef.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/acpi.h>
#include <kernel/printf.h>
#include <kernel/assert.h>

struct acpi_fadt *fadt = NULL;

__attribute__((no_sanitize("alignment")))
void fadt_init(void) {
#ifdef __x86_64__
    fadt = (struct acpi_fadt*)acpi_find_table("FACP");

    if (!fadt) return;

    if (fadt->smi_cmd != 0 || fadt->acpi_enable != 0 || fadt->acpi_disable != 0 || (fadt->pm1a_cnt_blk & 1) == 0) {
        dprintf("%s:%d: enabling ACPI... ", __FILE__, __LINE__);

        outb(fadt->smi_cmd, fadt->acpi_enable);
        while (!(inw(fadt->pm1a_cnt_blk) & 1));

        dprintf("done\n", __FILE__, __LINE__);
        return;
    }
    
    dprintf("%s:%d: ACPI is already enabled\n", __FILE__, __LINE__);
#else
    unimplemented;
#endif
}