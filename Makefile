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
    CCFLAGS := -m64 -std=gnu11 -g -ffreestanding -Wall -Wextra -nostdlib -Iinclude/ -fno-stack-protector -Wno-unused-parameter -fno-stack-check -fno-lto -mno-red-zone #-fsanitize=undefined
    LDFLAGS := -m elf_x86_64 -Tkernel/arch/x86_64/linker.ld -z noexecstack
    QEMUFLAGS := -cdrom bin/$(IMAGE_NAME).iso -boot d -M q35 -smp 2 -serial stdio -device ahci,id=ahci -drive id=disk,file=bin/image.hdd,if=none,format=raw -device ide-hd,drive=disk,bus=ahci.0
else ifeq ($(ARCH),riscv64)
	AS = riscv64-elf-as
	CC = riscv64-elf-gcc
	LD = riscv64-elf-ld
    ARCH_DIR := kernel/arch/riscv
    ASFLAGS :=
    CCFLAGS := -mcmodel=medany -ffreestanding -Wall -Wextra -nostdlib -Iinclude/ -fno-stack-protector -Wno-unused-parameter -fno-stack-check -fno-lto
    LDFLAGS := -m elf64lriscv -Tkernel/arch/riscv/linker.ld -z noexecstack
    QEMUFLAGS := -machine virt -bios none -kernel bin/$(IMAGE_NAME).elf -mon chardev=mon0,mode=readline,id=mon0 -chardev null,id=mon0 -display gtk
else
    $(error Unsupported architecture: $(ARCH))
endif

# Automatically find sources
KERNEL_S_SOURCES := $(shell find kernel -type f -name '*.S' ! -path "kernel/arch/*")
KERNEL_C_SOURCES := $(shell find kernel -type f -name '*.c' ! -path "kernel/arch/*")
ARCH_S_SOURCES   := $(shell find $(ARCH_DIR) -type f -name '*.S' | sed 's|^\./||')
ARCH_C_SOURCES   := $(shell find $(ARCH_DIR) -type f -name '*.c' | sed 's|^\./||') kernel/target_arch.c

# Get object files
KERNEL_OBJS := $(addprefix bin/, $(KERNEL_S_SOURCES:.S=.S.o) $(ARCH_S_SOURCES:.S=.S.o) $(KERNEL_C_SOURCES:.c=.c.o) $(ARCH_C_SOURCES:.c=.c.o))

all: kernel hdd iso

run: all
	@qemu-system-$(ARCH) $(QEMUFLAGS)

run-kvm: all
	@qemu-system-$(ARCH) $(QEMUFLAGS) -accel kvm

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

kernel/target_arch.c:
	@echo "const char *__kernel_arch = \"$(ARCH)\";" > $@

kernel: $(KERNEL_OBJS)
	@echo " LD kernel/*"
	@$(LD) $(LDFLAGS) $^ -o bin/$(IMAGE_NAME).elf
	@rm kernel/target_arch.c

ifeq ($(ARCH),x86_64)
iso:
	@grub-file --is-x86-multiboot2 ./bin/$(IMAGE_NAME).elf; \
	if [ $$? -eq 1 ]; then \
		echo " error: $(IMAGE_NAME).elf is not a valid multiboot2 file"; \
		exit 1; \
	fi
	@mkdir -p iso_root/boot/grub/
	@cp bin/$(IMAGE_NAME).elf iso_root/boot/$(IMAGE_NAME).elf
	@cp boot/grub.cfg iso_root/boot/grub/grub.cfg
	@grub-mkrescue -o bin/$(IMAGE_NAME).iso iso_root/ -quiet 2>&1 >/dev/null | grep -v libburnia | cat
	@rm -rf iso_root/
else ifeq ($(ARCH),riscv64)
iso:
else
    $(error Unsupported architecture: $(ARCH))
endif

hdd:
	@dd if=/dev/zero of=bin/image.hdd bs=1M count=64 status=none

clean:
	@rm -f $(BOOT_OBJS) $(KERNEL_OBJS)
	@rm -rf bin