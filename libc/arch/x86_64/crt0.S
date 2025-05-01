section .text.prologue
    global _start
    extern pre_main

_start:
    ;mov rdi, rsp
    ;mov rsi, [rsp + 8]
    call pre_main

section .note.GNU-stack noalloc noexec nowrite progbits