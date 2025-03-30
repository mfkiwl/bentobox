# bentobox
bentobox is a 64-bit SMP-enabled operating system targeting x86_64 and RISC-V

## Features on x86_64
- Supports for multiboot2
- 4-level paging with 48-bit addressing
- VGA text mode and serial driver
- ACPI table parsing
- LAPIC & IOAPIC support
- HPET timer support
- SMP-aware scheduler
- Node Graph VFS

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
- [X] `panic()` function
- [X] ANSI support in the VGA driver
- [X] Write a scheduler
    - [X] Mutexes
    - [ ] Semaphores
- [X] Write a VFS
- [X] FADT cleanup
- [X] PCI
- [X] SMP
- [ ] ATA/NVMe driver
- [ ] Proper ext2 driver
- [X] Framebuffer support
- [ ] PS/2 drivers
    - [ ] Keyboard
    - [ ] Mouse
- [ ] TSS and ring3
- [ ] ELF loading using the MMU
- [ ] Initial filesystem
- [X] `unimplemented` macro
- [ ] Symbol table
