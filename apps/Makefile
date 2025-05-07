# Toolchain
AS = nasm
CC = clang
LD = ld

# Automatically find sources
C_SRCS = $(wildcard *.c)
S_SRCS = $(wildcard *.S)
C_EXES = $(C_SRCS:.c=)
S_EXES = $(S_SRCS:.S=)

# Flags
CFLAGS = -ffreestanding -nostdlib -fno-pic -nostdlib -nostartfiles -fno-stack-protector -I../include/libc
ASFLAGS = -f elf64
LDFLAGS = -T../libc/arch/x86_64/linker.ld

# Output directory
OUT = ../base/bin

all: dirs $(addprefix $(OUT)/, $(S_EXES)) $(addprefix $(OUT)/, $(C_EXES))

dirs:
	@mkdir -p ../base/bin

$(OUT)/%: %.c
	@echo " CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@.o
	@$(LD) $(LDFLAGS) $@.o ../bin/libc.a -o $@
	@rm $@.o

$(OUT)/%: %.S
	@echo " AS $<"
	@$(AS) $(ASFLAGS) -o $@.o $<
	@$(LD) $(LDFLAGS) $@.o ../bin/libc.a -o $@
	@rm $@.o

clean:
	@rm -f $(addprefix $(OUT)/, $(EXES)) *.o

.PHONY: all clean