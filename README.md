# bentobox
bentobox is a 64-bit operating system targeting x86_64 and RISC-V

## Features on x86_64
- Supports the multiboot2 protocol
- 4-level paging with 48-bit addressing
- VGA text mode and serial driver
- ACPI table parsing
- APIC support
- HPET support

## Features on riscv
- Virtio UART driver

## Building (x86_64)
To build, you need to install the following packages:
- dev-essential
- nasm
- grub
- xorriso
- mtools
- qemu-system-x86

Then, you can simply run `make run` and the kernel will run in QEMU.