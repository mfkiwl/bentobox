#include <kernel/arch/x86_64/hpet.h>
#include <kernel/pci.h>
#include <kernel/mmu.h>
#include <kernel/printf.h>
#include <kernel/module.h>

#define AHCI_CAP        0x00    // Host Capabilities
#define AHCI_GHC        0x04    // Global Host Control
#define AHCI_IS         0x08    // Interrupt Status
#define AHCI_PI         0x0C    // Ports Implemented
#define AHCI_VS         0x10    // Version

static volatile uint32_t *ahci_base = NULL;

static uint32_t ahci_read_reg(uint32_t offset) {
    return ahci_base[offset / 4];
}

static void ahci_write_reg(uint32_t offset, uint32_t value) {
    ahci_base[offset / 4] = value;
}

int init() {
    dprintf("%s:%d: starting AHCI driver\n", __FILE__, __LINE__);

    struct pci_device *ahci_dev = pci_get_device(0x01, 0x06);
    if (!ahci_dev) {
        dprintf("%s:%d: No AHCI controller found!\n", __FILE__, __LINE__);
        return 1;
    }

    uint32_t cmd = pci_read(ahci_dev->bus, ahci_dev->device, 0, 0x04);
    cmd |= (1 << 8) | (1 << 2) | (1 << 1);
    pci_write(ahci_dev->bus, ahci_dev->device, 0, 0x04, cmd);

    uint32_t bar5 = pci_read(ahci_dev->bus, ahci_dev->device, 0, 0x24);
    uint32_t memory_type = bar5 & 0x6;
    uint64_t ahci_phys_base = 0;

    if (memory_type == 0x0) {
        ahci_phys_base = bar5 & 0xFFFFFFF0;
    } else if (memory_type == 0x4) {
        dprintf("%s:%d: 64-bit AHCI not implemented\n");
        return 1;
    }

    ahci_base = VIRTUAL(ahci_phys_base);
    mmu_map((void *)ahci_base, (void *)ahci_phys_base, PTE_PRESENT | PTE_WRITABLE); // TODO: PTE_CACHEABLE

    ahci_write_reg(AHCI_GHC, 1);

    hpet_sleep(1000); // 1ms

    uint32_t ghc = ahci_read_reg(AHCI_GHC);
    dprintf("new ghc: 0x%x\n", ghc);

    return 0;
}

int fini() {
    dprintf("%s:%d: Goodbye!\n", __FILE__, __LINE__);
    return 0;
}

struct Module metadata = {
    .name = "AHCI driver",
    .init = init,
    .fini = fini
};