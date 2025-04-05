#include <stddef.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/mmu.h>
#include <kernel/pci.h>
#include <kernel/ahci.h>
#include <kernel/printf.h>

struct pci_device *ahci_controller = NULL;
uintptr_t ahci_mmio_base = 0;

__attribute__((no_sanitize("undefined")))
uint32_t ahci_mmio_read(uint32_t reg) {
    return *((uint32_t*)(ahci_mmio_base + reg));
}

__attribute__((no_sanitize("undefined")))
void ahci_mmio_write(uint32_t reg, uint32_t value) {
    *((uint32_t*)(ahci_mmio_base + reg)) = value;
}

void reset_ahci_controller(void) {
    uint32_t ghc = ahci_mmio_read(0x04);
    ghc |= (1 << 0);
    ahci_mmio_write(0x04, ghc);

    while (ahci_mmio_read(0x04) & (1 << 0)) {
        asm volatile ("pause" : : : "memory");
    }

    printf("ahci: controller reset complete\n");
}

void ahci_install(void) {
    ahci_controller = pci_get_device(0x01, 0x06);

    if (!ahci_controller || pci_get_progif(ahci_controller->bus, ahci_controller->device, 0x00) != 0x01) {
        dprintf("%s:%d: AHCI controller not found\n", __FILE__, __LINE__);
        return;
    }
    printf("achi: found AHCI controller!\n");

    uint32_t command = pci_read(ahci_controller->bus, ahci_controller->device, 0x00, 0x04);
    command |= (1 << 1); /* Enable memory space */
    command |= (1 << 2); /* Enable Bus-Master DMA */
    command |= (1 << 0); /* Enable I/O space */
    pci_write(ahci_controller->bus, ahci_controller->device, 0x00, 0x04, command);

    printf("achi: enabled Memory Space, DMA and I/O space\n");

    uint32_t bar5 = pci_read(ahci_controller->bus, ahci_controller->device, 0x00, 0x24);
    if (!(bar5 & 0x1)) {
        printf("ahci: BAR5 is memory mapped\n");
        printf("ahci: MMIO base: 0x%x\n", ahci_mmio_base = bar5 & 0xFFFFFFF0);
        mmu_map(bar5, bar5, PTE_PRESENT | PTE_WRITABLE | PTE_CD | PTE_WT);
        printf("ahci: mapped BAR5\n");
    } else {
        printf("ahci: BAR5 is I/O mapped\n");
    }

    reset_ahci_controller();

    int msi_offset = pci_find_capability(ahci_controller, PCI_CAP_ID_MSI);
    if (!msi_offset) {
        printf("ahci: warning: PCI_CAP_ID_MSI not found\n");
    }

    printf("ahci: PCI_CAP_ID_MSI found at offset 0x%x\n", msi_offset);

    uint32_t msi_addr = LAPIC_REGS | (0 << 12);
    uint16_t msi_data = 0x50; // any vector > 0x20

    pci_write(ahci_controller->bus, ahci_controller->device, 0x00, msi_offset + 4, msi_addr);
    pci_write(ahci_controller->bus, ahci_controller->device, 0x00, msi_offset + 8, msi_data);

    uint16_t control = pci_read(ahci_controller->bus, ahci_controller->device, 0x00, msi_offset + 2) & 0xFFFF;
    control |= 1;
    pci_write(ahci_controller->bus, ahci_controller->device, 0x00, msi_offset + 2, control);

    printf("ahci: enabled MSI\n");
    return;
}