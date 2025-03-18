section .multiboot

MB2_MAGIC    equ 0xE85250D6
MB2_ARCH     equ 0
MB2_LENGTH   equ 24
MB2_CHECKSUM equ -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)

MB2_TAG_TYPE_END equ 0
MB2_TAG_SIZE_END equ 8

dd MB2_MAGIC
dd MB2_ARCH
dd MB2_LENGTH
dd MB2_CHECKSUM
dd MB2_TAG_TYPE_END
dd MB2_TAG_SIZE_END

section .text.prologue
    global _start
_start:
    cli
hltLoop:
    hlt
    jmp hltLoop