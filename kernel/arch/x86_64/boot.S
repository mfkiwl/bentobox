[BITS 32]

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
    extern kmain
    extern pml4
    extern pdp
    extern pd
_start:
    cli
    mov esp, stack_top

    push 0
    push ebx

    mov edi, pml4
    mov eax, pdp
    or eax, 0x7    ; PRESENT, WRITABLE, USER
    mov [edi], eax

    mov edi, pdp
    mov eax, pd
    or eax, 0x7    ; PRESENT, WRITABLE, USER
    mov [edi], eax

    mov edi, pd
    mov eax, 0x83  ; PRESENT, WRITABLE, HUGE
    mov ecx, 512
.set_entry:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .set_entry

    mov eax, cr4
    or eax, 1 << 5  ; PAE bit
    mov cr4, eax

    mov eax, pml4
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100   ; LME bit
    wrmsr

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    lgdt [gdtr]

    jmp 0x08:gdt2

[BITS 64]

gdt2:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    pop rdi
    call kmain

    cli
hltLoop:
    hlt
    jmp hltLoop

gdtr:
    dw gdt_end - gdt_base - 1
    dd gdt_base
    dd 0
gdt_base:
    ; null segment
    dq 0
    ; 64-bit segment
    dw 0
    dw 0
    db 0
    db 0x9a
    db 0x20
    db 0
    ; 64-bit segment
    dw 0xffff
    dw 0
    db 0
    db 0x92
    db 0
    db 0
gdt_end:

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: