# Toolchain
AS = nasm
CC = clang
LD = ld

# Automatically find sources
C_SRCS = $(wildcard *.c)
S_SRCS = $(wildcard *.S)
C_EXES = $(C_SRCS:.c=)
S_EXES = $(S_SRCS:.S=)

# Mlibc stuff
MLIBC_INCLUDE = -I../mlibc/ -I../mlibc/build/ -I../mlibc/options/ansi/libmlibc-musl-math.a.p -I../mlibc/options/ansi -I../mlibc/options/ansi -I../mlibc/options/internal/include -I../mlibc/options/elf/include -I../mlibc/options/lsb/include -I../mlibc/options/rtld/include -I../mlibc/options/rtld/x86_64 -I../mlibc/options/internal/x86_64-I../mlibc/nclude -I../mlibc/sysdeps/generic-helpers/include -I../mlibc/sysdeps/bentobox/include -I../mlibc/options/ansi/include -I../mlibc/subprojects/freestnd-c-hdrs/x86_64/include -I../mlibc/subprojects/freestnd-cxx-hdrs/x86_64/include -I../mlibc/subprojects/frigg/include
MLIBC_OBJ = ../mlibc/build/sysdeps/bentobox/crt0.o ../mlibc/build/sysdeps/bentobox/crti.o ../mlibc/build/sysdeps/bentobox/crtn.o ../mlibc/build/libc.a

# Flags
CFLAGS = -ffreestanding -nostdlib -fno-pic -nostdlib -fno-stack-protector -mcmodel=large $(MLIBC_INCLUDE)
ASFLAGS = -f elf64
LDFLAGS =

# Output directory
OUT = ../base/bin

all: dirs $(addprefix $(OUT)/, $(S_EXES)) $(addprefix $(OUT)/, $(C_EXES))

dirs:
	@mkdir -p ../base/bin

$(OUT)/%: %.c
	@echo " CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@.o
	@$(LD) $(LDFLAGS) $@.o $(MLIBC_OBJ) -o $@
	@rm $@.o

$(OUT)/%: %.S
	@echo " AS $<"
	@$(AS) $(ASFLAGS) -o $@.o $<
	@$(LD) $(LDFLAGS) $@.o -o $@
	@rm $@.o

clean:
	@rm -rf ../base/bin

.PHONY: all clean