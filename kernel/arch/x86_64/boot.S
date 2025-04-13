[BITS 32]

section .text.prologue
    global _start
    global gdtr
    global stack_top
    extern kmain

[BITS 64]

gdt2:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    pop rdi
    pop rsi
    call kmain

hltLoop:
    cli
    hlt
    jmp hltLoop
gdt_end:

section .text
    extern gdt_descriptor
    global gdt_flush
    global arch_fatal

gdt_flush:
    jmp $

global flush_tss
flush_tss:
    mov ax, (5 * 8) | 0
    ltr ax
    ret

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: