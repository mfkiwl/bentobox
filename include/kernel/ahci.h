#pragma once

#define AHCI_GHC_CTRL       0x00
#define AHCI_GHC_CTRL_RESET 0x01

#define AHCI_PORTS_IMPL     0x04
#define AHCI_PORT_CMD       0x18
#define AHCI_PORT_CMD_START 0x01
#define AHCI_PORT_CMD_ST    0x02

void ahci_install(void);