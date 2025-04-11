# bentobox
bentobox is a 64-bit SMP-enabled operating system targeting x86_64 and RISC-V

## Features on x86_64
- Multiboot 2 boot & module support
- 4-level paging
- VGA text mode and serial driver
- ACPI table parsing
- LAPIC & IOAPIC support
- HPET timer support
- SMP-aware scheduler
- Node Graph VFS
- ELf64 loading

## Features on riscv
- Virtio UART driver

> [!NOTE]
> bentobox on RISC-V is a stub

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
- [X] Write a VFS
- [X] FADT cleanup
- [X] PCI
- [X] SMP
- [ ] AHCI driver
- [ ] ext2fs support
- [X] Framebuffer support
- [ ] PS/2 drivers
    - [X] Keyboard
    - [ ] Mouse
- [ ] Userspace support
    - [X] TSS
    - [X] Ring 3 in the scheduler
    - [ ] Port a libc
- [X] ELF loading
- [X] Symbol table
- [ ] Initial filesystem
- [X] `unimplemented` macro
- [X] Simplify the PCI driver
- [X] FIFO queues
