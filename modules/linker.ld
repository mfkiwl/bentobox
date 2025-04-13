SECTIONS
{
    . = text_ld;
    . (NOLOAD) : { bin/ksym.elf(.text) }
    . = rodata_ld;
    . (NOLOAD) : { bin/ksym.elf(.rodata) }
    . = data_ld;
    . (NOLOAD) : { bin/ksym.elf(.data) }
    . = bss_ld;
    . (NOLOAD) : { bin/ksym.elf(.bss) }

    . = 0x400000;
    . : { bin/module.elf(.text) }
    . : { bin/module.elf(.rodata) }
    . : { bin/module.elf(.data) }
    . : { bin/module.elf(.bss) }
}