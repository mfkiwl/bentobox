[BITS 16]

section .text
    global _L8000_ap_trampoline
    extern initial_pml
    extern ap_startup
    extern gdtr
    extern mmu_alloc

_L8000_ap_trampoline:
    jmp $