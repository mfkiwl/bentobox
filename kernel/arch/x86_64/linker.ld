ENTRY(_start)

SECTIONS
{
    . = 1M;

    text_start_ld = .;
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text.prologue)
        *(.text)
    }
    text_end_ld = .;

    rodata_start_ld = .;
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }
    rodata_end_ld = .;

    data_start_ld = .;
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }
    data_end_ld = .;

    bss_start_ld = .;
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
    bss_end_ld = .;

    end = ALIGN(4K);

    /DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.build-id)
	}
}