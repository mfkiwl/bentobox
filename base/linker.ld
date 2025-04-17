ENTRY(_start)

SECTIONS
{
    . = 0x600000;

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }
}