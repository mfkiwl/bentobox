# Toolchain
CC = clang
LD = ld

# Automatically find sources
SRCS = $(wildcard *.c)
EXES = $(SRCS:.c=)

# Flags
CFLAGS = -ffreestanding -nostdlib -fno-pic -nostdlib -nostartfiles -fno-stack-protector -I../include/libc
LDFLAGS = -T../libc/arch/x86_64/linker.ld

# Output directory
OUT = ../base/bin

all: dirs $(addprefix $(OUT)/, $(EXES))

dirs:
	@mkdir -p ../base/bin

$(OUT)/%: %.c
	@echo " CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@.o
	@$(LD) $(LDFLAGS) $@.o ../bin/libc.a -o $@
	@rm $@.o

clean:
	@rm -f $(addprefix $(OUT)/, $(EXES)) *.o

.PHONY: all clean