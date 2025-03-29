#pragma once
#include <stdint.h>

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

extern char ata_drive_name[];

void ata_install(void);
uint8_t ata_read(uint32_t lba, uint8_t *buffer, uint32_t sectors);
uint8_t ata_write(uint32_t lba, uint8_t *buffer, uint32_t sectors);