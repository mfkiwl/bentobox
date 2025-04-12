ENTRY(_start)

SECTIONS
{
    . = 1M;

    text_start_ld = .;
    .text :
    {
        *(.multiboot)
        *(.text.prologue)
        *(.text)
    }
    text_end_ld = .;

    . = ALIGN(4K);

    rodata_start_ld = .;
    .rodata :
    {
        *(.rodata)
    }
    rodata_end_ld = .;

    . = ALIGN(4K);

    data_start_ld = .;
    .data :
    {
        *(.data)
    }
    data_end_ld = .;

    . = ALIGN(4K);

    bss_start_ld = .;
    .bss :
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