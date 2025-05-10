ENTRY(_start)

SECTIONS
{
    . = 0x4000000;

    .text : {
        *(.text.prologue)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }
}