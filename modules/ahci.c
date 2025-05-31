#include "kernel/malloc.h"
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/pci.h>
#include <kernel/mmu.h>
#include <kernel/printf.h>
#include <kernel/module.h>
#include <kernel/string.h>

#define AHCI_CAP        0x00    // Host Capabilities
#define AHCI_GHC        0x04    // Global Host Control
#define AHCI_IS         0x08    // Interrupt Status
#define AHCI_PI         0x0C    // Ports Implemented
#define AHCI_VS         0x10    // Version

#define AHCI_DEV_NULL   0
#define AHCI_DEV_SATA   1
#define AHCI_DEV_SATAPI 2
#define AHCI_DEV_SEMB   3
#define AHCI_DEV_PM     4

#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	    0x96690101	// Port multiplier

#define GHC_AHCI_ENABLE (1 << 31)   // AHCI Enable
#define GHC_MRSM        (1 << 2)    // MSI Revert to Single Message
#define GHC_IE          (1 << 1)    // Interrupt Enable
#define GHC_HR          (1 << 0)    // HBA Reset

#define HBA_PORT_DET_PRESENT    3
#define HBA_PORT_IPM_ACTIVE     1
#define HBA_CMD_ST      (1 << 0)    // Start
#define HBA_CMD_FRE     (1 << 4)    // FIS Receive Enable
#define HBA_CMD_CR      (1 << 15)   // Command List Running
#define HBA_CMD_FR      (1 << 14)   // FIS Receive Running

#define PORT_CLB        0x00    // Command List Base Address
#define PORT_CLBU       0x04    // Command List Base Address Upper
#define PORT_FB         0x08    // FIS Base Address
#define PORT_FBU        0x0C    // FIS Base Address Upper
#define PORT_IS         0x10    // Interrupt Status
#define PORT_IE         0x14    // Interrupt Enable
#define PORT_CMD        0x18    // Command and Status
#define PORT_TFD        0x20    // Task File Data
#define PORT_SIG        0x24    // Signature
#define PORT_SSTS       0x28    // SATA Status
#define PORT_SCTL       0x2C    // SATA Control
#define PORT_SERR       0x30    // SATA Error
#define PORT_SACT       0x34    // SATA Active
#define PORT_CI         0x38    // Command Issue

#define LOW(x)  ((uint32_t)(x))
#define HIGH(x) ((uint32_t)((x) >> 32))

typedef struct {
    int port_num;
    void *clb;          // Command List Base (virtual address)
    void *fb;           // FIS Base (virtual address)  
    void *cmd_tbls[32]; // Command Tables (virtual addresses)
    uint64_t clb_phys;  // Command List Base (physical address)
    uint64_t fb_phys;   // FIS Base (physical address)
} ahci_port_t;

// Command header structure (goes in command list)
typedef struct {
    uint8_t cfl:5;      // Command FIS length in DWORDS, 2 ~ 16
    uint8_t a:1;        // ATAPI
    uint8_t w:1;        // Write, 1: H2D, 0: D2H
    uint8_t p:1;        // Prefetchable
    
    uint8_t r:1;        // Reset
    uint8_t b:1;        // BIST
    uint8_t c:1;        // Clear busy upon R_OK
    uint8_t rsv0:1;     // Reserved
    uint8_t pmp:4;      // Port multiplier port
    
    uint16_t prdtl;     // Physical region descriptor table length in entries
    
    volatile uint32_t prdbc;  // Physical region descriptor byte count transferred
    
    uint32_t ctba_low;  // Command table descriptor area base address low
    uint32_t ctba_high; // Command table descriptor area base address high
    
    uint32_t rsv1[4];   // Reserved
} __attribute__((packed)) hba_cmd_header_t;

static volatile uint32_t *ahci_base = NULL;
int command_slots = 0, connected_ports = 0;
ahci_port_t *ahci_ports[32];

static uint32_t ahci_read_reg(uint32_t offset) {
    return ahci_base[offset / 4];
}

static void ahci_write_reg(uint32_t offset, uint32_t value) {
    ahci_base[offset / 4] = value;
}

static uint32_t port_read_reg(int port, uint32_t offset) {
    uint32_t port_base = 0x100 + (port * 0x80);
    return ahci_read_reg(port_base + offset);
}

static void port_write_reg(int port, uint32_t offset, uint32_t value) {
    uint32_t port_base = 0x100 + (port * 0x80);
    ahci_write_reg(port_base + offset, value);
}

ahci_port_t *ahci_init_disk(int port_num) {
    ahci_port_t *ahci_port = (ahci_port_t *)kmalloc(sizeof(ahci_port_t));
    ahci_port->port_num = port_num;

    uint32_t cmd = port_read_reg(port_num, PORT_CMD);
    cmd &= ~HBA_CMD_ST;
    port_write_reg(port_num, PORT_CMD, cmd);

    int timeout = 500;
    while (timeout-- > 0) {
        cmd = port_read_reg(port_num, PORT_CMD);
        if (!(cmd & HBA_CMD_CR)) {
            break;
        }
        hpet_sleep(1000);
    }

    if (timeout <= 0) {
        dprintf("%s:%d: Failed to stop command engine on port %d\n", port_num);
        kfree(ahci_port);
        return NULL;
    }

    void *clb = VIRTUAL_IDENT(mmu_alloc(1));
    ahci_port->clb_phys = (uint64_t)PHYSICAL_IDENT(clb);
    ahci_port->clb = clb;

    port_write_reg(port_num, PORT_CLB, LOW(ahci_port->clb_phys));
    port_write_reg(port_num, PORT_CLBU, HIGH(ahci_port->clb_phys));
    memset(clb, 0, PAGE_SIZE);

    void *fb = VIRTUAL_IDENT(mmu_alloc(1));
    ahci_port->fb_phys = (uint64_t)PHYSICAL_IDENT(fb);
    ahci_port->fb = fb;

    port_write_reg(port_num, PORT_FB, LOW(ahci_port->fb_phys));
    port_write_reg(port_num, PORT_FBU, HIGH(ahci_port->fb_phys));
    memset(fb, 0, PAGE_SIZE);

    hba_cmd_header_t *cmd_hdr = (hba_cmd_header_t *)clb;
    for (int i = 0; i < 32; i++) {
        cmd_hdr[i].prdtl = 8;

        void *cmd_tbl = VIRTUAL_IDENT(mmu_alloc(1));
        uint64_t cmd_tbl_phys = (uint64_t)PHYSICAL_IDENT(cmd_tbl);
        cmd_hdr[i].ctba_low = LOW(cmd_tbl_phys);
        cmd_hdr[i].ctba_high = HIGH(cmd_tbl_phys);
        memset(cmd_tbl, 0, PAGE_SIZE);

        ahci_port->cmd_tbls[i] = cmd_tbl;
    }

    port_write_reg(port_num, PORT_SERR, 0xFFFFFFFF);

    cmd = port_read_reg(port_num, PORT_CMD);
    cmd |= HBA_CMD_FRE;
    port_write_reg(port_num, PORT_CMD, cmd);

    timeout = 500;
    while (timeout-- > 0) {
        cmd = port_read_reg(port_num, PORT_CMD);
        if (cmd & HBA_CMD_FR) {
            break;
        }
        hpet_sleep(1000);
    }

    cmd |= HBA_CMD_ST;
    port_write_reg(port_num, PORT_CMD, cmd);

    dprintf("%s:%d: initialized port %d\n", __FILE__, __LINE__, port_num);
    return ahci_port;
}

uint32_t ahci_check_type(int port) {
    uint32_t ssts = port_read_reg(port, PORT_SSTS);
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;
    
    if (det != HBA_PORT_DET_PRESENT)
        return AHCI_DEV_NULL;
    if (ipm != HBA_PORT_IPM_ACTIVE)
        return AHCI_DEV_NULL;
        
    uint32_t sig = port_read_reg(port, PORT_SIG);
    switch (sig) {
        case SATA_SIG_ATAPI:
            return AHCI_DEV_SATAPI;
        case SATA_SIG_SEMB:
            return AHCI_DEV_SEMB;
        case SATA_SIG_PM:
            return AHCI_DEV_PM;
        default:
            return AHCI_DEV_SATA;
    }
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
        dprintf("%s:%d: FATAL: 64-bit AHCI not implemented\n");
        return 1;
    }

    ahci_base = VIRTUAL(ahci_phys_base);
    mmu_map_pages(2, (void *)ahci_base, (void *)ahci_phys_base, PTE_PRESENT | PTE_WRITABLE); // TODO: PTE_CACHEABLE

    uint32_t ghc = ahci_read_reg(AHCI_GHC);
    ghc |= GHC_HR;
    ahci_write_reg(AHCI_GHC, ghc);

    int i;
    for (i = 0; i < 1000; i++) {
        ghc = ahci_read_reg(AHCI_GHC);
        if (!(ghc & GHC_HR)) {
            break;
        }
        hpet_sleep(1000);
    }

    if (i + 1 >= 1000) {
        dprintf("%s:%d: FATAL: failed to reset controller: timed out\n");
        return 1;
    }

    ghc = ahci_read_reg(AHCI_GHC);
    if (!(ghc & GHC_AHCI_ENABLE)) {
        ghc |= GHC_AHCI_ENABLE;
        hpet_sleep(1000);
        ghc = ahci_read_reg(AHCI_GHC);
        if (!(ghc & GHC_AHCI_ENABLE)) {
            dprintf("%s:%d: FATAL: failed to enable AHCI mode!\n");
            return 1;
        }
    } else {
        printf("AHCI already enabled\n");
    }

    uint32_t cap = ahci_read_reg(AHCI_CAP);
    uint8_t command_slots = ((cap >> 8) & 0x1F) + 1;
    dprintf("%s:%d: %d command slots available\n", __FILE__, __LINE__, command_slots);

    hpet_sleep(5000);
    ahci_write_reg(AHCI_IS, 0xFFFFFFFF); // clear interrupt

    uint32_t pi = ahci_read_reg(AHCI_PI);
    for (int i = 0; i < 32; i++) {
        if (pi & 1) {
            uint32_t type = ahci_check_type(i);
            if (type == AHCI_DEV_SATA) {
                ahci_ports[connected_ports++] = ahci_init_disk(i);
                dprintf("%s:%d: port %d is a SATA device\n", __FILE__, __LINE__, i);
            }
        }
        pi >>= 1;
    }

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