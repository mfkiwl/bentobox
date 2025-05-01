# Toolchain
AS = nasm
CC = clang

# Automatically find sources
S_SOURCES = $(shell find . -type f -name '*.S')
C_SOURCES = $(shell find . -type f -name '*.c')

# Get object files
OBJS := $(patsubst ./%, ../bin/libc/%, $(S_SOURCES:.S=.S.o) $(C_SOURCES:.c=.c.o))

# Flags
ASFLAGS = -f elf64
CCFLAGS = -m64 -std=gnu11 -g -ffreestanding -O0 -Wall -Wextra -nostdlib -fno-stack-protector -Wno-unused-parameter -I../include/

all: libc

../bin/libc/%.c.o: %.c
	@echo " CC $<"
	@mkdir -p "$$(dirname $@)"
	@$(CC) $(CCFLAGS) -c $< -o $@

../bin/libc/%.S.o: %.S
	@echo " AS $<"
	@mkdir -p "$$(dirname $@)"
	@$(AS) $(ASFLAGS) -o $@ $<

libc: $(OBJS)
	@echo " AR ../bin/libc.a"
	@ar rcs ../bin/libc.a $^

clean:
	@rm -f $(OBJS)
	@rm -f ../bin/libc.a
	@rm -rf ../bin/libc