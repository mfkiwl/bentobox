#include <stdint.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/ata.h>
#include <kernel/vfs.h>
#include <kernel/printf.h>

uint16_t ata_base;
uint8_t ata_type;
char ata_drive_name[41];

struct vfs_node *ata_dev = NULL;

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

uint8_t ata_identify(uint16_t base, uint8_t type) {
    ata_base = base;
    ata_type = type;

    outb(base + 6, type); /* select master/slave */
    for (uint16_t i = 0x1F2; i != 0x1F5; i++) {
        outb(i, 0);
    }
    outb(base + 7, ATA_IDENTIFY); /* send identify command */

    uint8_t status = inb(base + 7);
    if (!status) {
        return ATA_NO_DRIVES;
    }
    if (ata_poll() != ATA_OK) {
        return ATA_DISK_ERR;
    }

    uint8_t ident_buf[512];
    ata_read(0, ident_buf, 1);

    /* reverse the buffer & store the drive name */
    for (uint8_t i = 0; i < 40; i += 2) {
        ata_drive_name[i] = ident_buf[54 + i + 1];
        ata_drive_name[i + 1] = ident_buf[54 + i];
    }

    /* add null terminator to the drive name */
    for (uint8_t i = 39; i > 0; i--) {
        if (ata_drive_name[i] != ' ') {
            ata_drive_name[i + 1] = 0;
            break;
        }
    }

    ata_400ns();
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

int32_t atafs_read(struct vfs_node *node, void *buffer, uint32_t len) {
    int ret = ata_read(len, buffer, 1);
    return ret == ATA_OK ? 512 : ret;
}

void ata_install(void) {
    if (!ata_identify(ATA_PRIMARY, ATA_MASTER)) {
        printf("\033[92m * \033[97mInitialized ATA Primary Master\033[0m\n");
    }

    ata_dev = vfs_create_node("hda", VFS_CHARDEVICE);
    ata_dev->write = NULL;
    ata_dev->read = atafs_read;
    vfs_add_node(vfs_dev, ata_dev);
}