#pragma once
#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t class;
    uint8_t subclass;
    uint16_t vendor_id;
    uint16_t device_id;
};

extern struct pci_device primary_bus[32];

void pci_scan(void);
void pci_check_function(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_irq_line(uint8_t bus, uint8_t device, uint8_t function);
uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
struct pci_device *pci_get_device(uint8_t class, uint8_t subclass);
struct pci_device *pci_get_device_from_vendor(uint16_t vendor, uint16_t device);