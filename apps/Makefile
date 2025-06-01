# Toolchain
AS = nasm
CC = clang
LD = ld

# Automatically find sources
C_SRCS = $(wildcard *.c)
S_SRCS = $(wildcard *.S)
C_EXES = $(C_SRCS:.c=)
S_EXES = $(S_SRCS:.S=)

SYSROOT = /opt/mlibc
OBJ = $(SYSROOT)/lib/crt0.o $(SYSROOT)/lib/crti.o $(SYSROOT)/lib/crtn.o $(SYSROOT)/lib/libc.a

# Flags
CFLAGS = -I$(SYSROOT)/include -ffreestanding -nostdlib -fno-pic -fno-stack-protector -mcmodel=large -g
ASFLAGS = -f elf64
LDFLAGS =

# Output directory
OUT = ../bin/base/bin

all: dirs $(addprefix $(OUT)/, $(S_EXES)) $(addprefix $(OUT)/, $(C_EXES))

dirs:
	@mkdir -p $(OUT)

$(OUT)/%: %.c
	@echo " CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@.o
	@$(LD) $(LDFLAGS) $@.o $(OBJ) -o $@
	@rm $@.o

$(OUT)/%: %.S
	@echo " AS $<"
	@$(AS) $(ASFLAGS) -o $@.o $<
	@$(LD) $(LDFLAGS) $@.o -o $@
	@rm $@.o

.PHONY: all clean