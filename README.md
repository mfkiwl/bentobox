# bentobox
bentobox is a 64-bit operating system targeting x86_64 and RISC-V

## Features on x86_64
- Supports the multiboot2 protocol
- 4-level paging with 48-bit addressing
- VGA text mode and serial driver
- ACPI table parsing
- LAPIC & IOAPIC support
- PIT support

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

## TODO
- [ ] `panic()` function
- [ ] ANSI support in the VGA driver
- [X] Write a scheduler
    - [X] Mutexes
    - [ ] Semaphores
- [ ] Write a VFS
- [X] FADT cleanup
- [ ] PCI driver
- [ ] ATAPIO/NVMe driver
- [ ] Proper ext2 driver
- [ ] Framebuffer support
    - [ ] Multiboot2 framebuffer (VBE)
    - [ ] VMware SVGAII driver
    - [ ] Framebuffer console
- [ ] PS/2 drivers
    - [ ] Keyboard
    - [ ] Mouse
- [ ] TSS and ring3
- [ ] ELF loading using the MMU
- [ ] Initial filesystem
- [X] `unimplemented` macro
