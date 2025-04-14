# Target architecture
ARCH ?= x86_64

# Output image name
IMAGE_NAME = image

# Architecture specific
ifeq ($(ARCH),x86_64)
	AS = nasm
	CC = clang
	LD = ld
    ARCH_DIR := kernel/arch/x86_64
    ASFLAGS = -f elf64 -g -F dwarf
    CCFLAGS := -m64 -march=x86-64 -std=gnu11 -g -ffreestanding -Wall -Wextra -nostdlib -Iinclude/ -fno-stack-protector -Wno-unused-parameter -fno-stack-check -fno-lto -mno-red-zone #-fsanitize=undefined
    LDFLAGS := -m elf_x86_64 -nostdlib -static -pie --no-dynamic-linker -z text -z max-page-size=0x1000 -T kernel/arch/x86_64/linker.ld
    QEMUFLAGS := -serial stdio -cdrom bin/$(IMAGE_NAME).iso -boot d -drive file=bin/$(IMAGE_NAME).hdd,format=raw -d int -M smm=off -no-reboot -no-shutdown
else ifeq ($(ARCH),riscv64)
	AS = riscv64-elf-as
	CC = riscv64-elf-gcc
	LD = riscv64-elf-ld
    ARCH_DIR := kernel/arch/riscv
    ASFLAGS :=
    CCFLAGS := -mcmodel=medany -ffreestanding -Wall -Wextra -nostdlib -Iinclude/ -fno-stack-protector -Wno-unused-parameter -fno-stack-check -fno-lto
    LDFLAGS := -m elf64lriscv -Tkernel/arch/riscv/linker.ld -z noexecstack
    QEMUFLAGS := -machine virt -bios none -kernel bin/kernel.elf -mon chardev=mon0,mode=readline,id=mon0 -chardev null,id=mon0 -display gtk
else
    $(error Unsupported architecture: $(ARCH))
endif

# Automatically find sources
KERNEL_S_SOURCES := $(shell find kernel -type f -name '*.S' ! -path "kernel/arch/*")
KERNEL_C_SOURCES := $(shell find kernel -type f -name '*.c' ! -path "kernel/arch/*")
MODULE_C_SOURCES := $(shell find modules -type f -name '*.c')
ARCH_S_SOURCES   := $(shell find $(ARCH_DIR) -type f -name '*.S' | sed 's|^\./||')
ARCH_C_SOURCES   := $(shell find $(ARCH_DIR) -type f -name '*.c' | sed 's|^\./||') kernel/target_arch.c

# Get object files
KERNEL_OBJS := $(addprefix bin/, $(KERNEL_S_SOURCES:.S=.S.o) $(ARCH_S_SOURCES:.S=.S.o) $(KERNEL_C_SOURCES:.c=.c.o) $(ARCH_C_SOURCES:.c=.c.o))
MODULE_OBJS := $(addprefix bin/, $(MODULE_C_SOURCES:.c=.o))

# Get module binaries
MODULE_BINARIES := $(addprefix bin/, $(MODULE_C_SOURCES:.c=.elf))

.PHONY: all
all: kernel/target_arch.c kernel iso hdd

.PHONY: run
run: all
	@qemu-system-$(ARCH) $(QEMUFLAGS)

.PHONY: run-kvm
run-kvm: all
	@qemu-system-$(ARCH) $(QEMUFLAGS) -smp 4 -accel kvm

.PHONY: run-gdb
run-gdb: all
	@qemu-system-$(ARCH) $(QEMUFLAGS) -S -s

bin/kernel/%.c.o: kernel/%.c
	@echo " CC $<"
	@mkdir -p "$$(dirname $@)"
	@$(CC) $(CCFLAGS) -c $< -o $@

bin/kernel/%.S.o: kernel/%.S
	@echo " AS $<"
	@mkdir -p "$$(dirname $@)"
	@$(AS) $(ASFLAGS) -o $@ $<

kernel/target_arch.c: bin/.target
	@echo "const char *__kernel_arch = \"$(ARCH)\";" > $@
	@echo "const char *__kernel_commit_hash = \"$(shell git rev-parse --short HEAD)\";" >> $@

bin/.target: kernel/target_arch.c
	mkdir -p "$$(dirname $@)"
	@touch $@

bin/modules/%.o: modules/%.c $(KERNEL_OBJS)
	@echo " CC $<"
	@mkdir -p "$$(dirname $@)"
	@$(CC) $(CCFLAGS) -c $< -o $@

.PHONY: kernel
kernel: $(KERNEL_OBJS)
	@echo " LD kernel/*"
	@$(LD) $(LDFLAGS) $^ -o bin/kernel.elf
	@objcopy --only-keep-debug bin/kernel.elf bin/ksym.elf
	@bash util/symbols.sh

.PHONY: modules
modules: kernel $(MODULE_OBJS)
	@for obj in $(MODULE_OBJS); do \
		echo " LD $${obj}"; \
		cp $${obj} bin/module.elf; \
		ld -Tbin/mod.ld bin/ksym.elf bin/module.elf -o $${obj%.o}.elf; \
	done

include/limine.h:
	@echo " GET https://github.com/limine-bootloader/limine/raw/v9.x/limine.h"
	@curl -Lo include/limine.h https://github.com/limine-bootloader/limine/raw/v9.x/limine.h

limine:
	git clone https://github.com/limine-bootloader/limine.git --branch=v9.x-binary --depth=1
	make -C limine CC="cc" CFLAGS="-g -O2 -pipe" CPPFLAGS="" LDFLAGS="" LIBS=""

.PHONY: iso
ifeq ($(ARCH),x86_64)
iso: kernel limine
	@rm -rf iso_root
	@mkdir -p iso_root/boot
	@cp -a bin/kernel.elf iso_root/boot/kernel.elf
	@mkdir -p iso_root/boot/limine
	@cp boot/limine.conf limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/boot/limine/
	@mkdir -p iso_root/EFI/BOOT
	@cp limine/BOOTX64.EFI iso_root/EFI/BOOT/
	@cp limine/BOOTIA32.EFI iso_root/EFI/BOOT/
	@xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o bin/$(IMAGE_NAME).iso -quiet 2>&1 >/dev/null \
		| grep -v libburnia | cat
	@./limine/limine bios-install bin/$(IMAGE_NAME).iso 2>&1 >/dev/null | grep -Ev \
		"Physical|Installing|Secondary|partition|Stage|Reminder|Limine|directories" | cat
	@rm -rf iso_root
else ifeq ($(ARCH),riscv64)
iso:
else
    $(error Unsupported architecture: $(ARCH))
endif

.PHONY: hdd
hdd:
	@dd if=/dev/zero of=bin/$(IMAGE_NAME).hdd bs=1M count=64 status=none
	@mkfs.ext2 bin/$(IMAGE_NAME).hdd -L bentobox 2>&1 >/dev/null | grep -v mke2fs | cat

.PHONY: clean
clean:
	@rm -f $(BOOT_OBJS) $(KERNEL_OBJS)
	@rm -rf bin