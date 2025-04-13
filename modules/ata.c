#include <kernel/arch/x86_64/io.h>
#include <kernel/pci.h>
#include <kernel/mmu.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/module.h>

#define ATA_PRIMARY         0x1F0
#define ATA_SECONDARY       0x170
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_CTRL  0x376
#define ATA_MASTER          0xA0
#define ATA_SLAVE           0xB0
#define ATA_WAIT            0x00
#define ATA_IDENTIFY        0xEC
#define ATA_READ            0x20
#define ATA_WRITE           0x30
#define ATA_OK              0x00
#define ATA_NO_DRIVES       0x01
#define ATA_DISK_ERR        0x02

uint16_t ata_base;
uint8_t ata_type;

uint8_t *ata_ident = NULL;

void ata_400ns(void) {
    for (int i = 0; i < 4; i++) {
        inb(ata_base + 7);
    }
}

uint8_t ata_poll() {
    ata_400ns();

    uint8_t status = 0;
    for (;;) {
        status = inb(ata_base + 7);
        if (!(status & 0x80)) {
            break;
        } 
        if (status & 0x08) {
            break;
        }
        if (status & 0x01) {
            return ATA_DISK_ERR;
        }
    }

    return ATA_OK;
}

__attribute__((no_sanitize("undefined")))
uint8_t ata_read(uint32_t lba, uint8_t *buffer, uint32_t sectors) {
    outb(ata_base + 6, (ata_type == ATA_MASTER ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));
    outb(ata_base + 1, ATA_WAIT);
    outb(ata_base + 2, sectors);
    outb(ata_base + 3, (uint8_t)lba);
    outb(ata_base + 4, (uint8_t)(lba >> 8));
    outb(ata_base + 5, (uint8_t)(lba >> 16));
    outb(ata_base + 7, ATA_READ);

    uint16_t *buf = (uint16_t *)buffer;
    for (uint32_t i = 0; i < sectors * 256; i++) {
        if (ata_poll() != ATA_OK) {
            return ATA_DISK_ERR;
        }
        buf[i] = inw(ata_base);
    }

    ata_400ns();
    return ATA_OK;
}

__attribute__((no_sanitize("undefined")))
uint8_t ata_write(uint32_t lba, uint8_t *buffer, uint32_t sectors) {
    outb(ata_base + 6, (ata_type == ATA_MASTER ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));
    outb(ata_base + 1, ATA_WAIT);
    outb(ata_base + 2, sectors);
    outb(ata_base + 3, (uint8_t)lba);
    outb(ata_base + 4, (uint8_t)(lba >> 8));
    outb(ata_base + 5, (uint8_t)(lba >> 16));
    outb(ata_base + 7, ATA_WRITE);

    uint16_t *buf = (uint16_t *)buffer;
    for (uint32_t i = 0; i < sectors * 256; i++) {
        if (ata_poll() != ATA_OK) {
            return ATA_DISK_ERR;
        }
        outw(ata_base, buf[i]);
    }

    ata_400ns();
    return ATA_OK;
}

uint8_t ata_identify(uint16_t base, uint8_t type, char *name) {
    ata_base = base;
    ata_type = type;

    outb(base + 6, type); /* select drive */
    for (uint16_t i = 0x1F2; i != 0x1F5; i++) {
        outb(i, 0);
    }
    outb(base + 7, ATA_IDENTIFY);

    uint8_t status = inb(base + 7);
    if (!status) {
        return ATA_NO_DRIVES;
    }
    if (ata_poll() != ATA_OK) {
        return ATA_DISK_ERR;
    }

    ata_ident = mmu_alloc(1);
    mmu_map((uintptr_t)VIRTUAL(ata_ident), (uintptr_t)ata_ident, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    ata_read(0, ata_ident, 1);
    mmu_map((uintptr_t)VIRTUAL(ata_ident), (uintptr_t)ata_ident, PTE_PRESENT | PTE_USER);

    uint8_t i = 0;
    for (i = 0; i < 40; i += 2) {
        name[i] = ata_ident[54 + i + 1];
        name[i + 1] = ata_ident[54 + i];
    }
    for (i = 39; i > 0; i--) {
        if (name[i] != ' ') {
            name[i + 1] = 0;
            break;
        }
    }

    ata_400ns();
    return ATA_OK;
}

int32_t hda_write(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len) {
    if (len == 0) return 0;
    return ata_write(offset / 512, buffer, len / 512) ? 0 : len;
}

int32_t hda_read(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len) {
    if (len == 0) return 0;
    return ata_read(offset / 512, buffer, len / 512) ? 0 : len;
}

int init() {
    sched_stop_timer();
    dprintf("%s:%d: bentobox ATA driver v1.0\n", __FILE__, __LINE__);

    char name[40];
    if (ata_identify(ATA_PRIMARY, ATA_MASTER, name) == ATA_OK) {
        dprintf("%s:%d: Drive name: %s\n", __FILE__, __LINE__, name);
    }

    struct vfs_node *hda = vfs_create_node("hda", VFS_BLOCKDEVICE);
    hda->read = hda_read;
    hda->write = hda_write;
    vfs_add_device(hda);

    sched_start_timer();
    return 0;
}

struct Module metadata = {
    .name = "ATA driver",
    .init = init
};