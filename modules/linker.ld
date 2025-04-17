SECTIONS
{
    . = text_ld;
    . (NOLOAD) : { bin/ksym_rel.elf(.text) }
    . = rodata_ld;
    . (NOLOAD) : { bin/ksym_rel.elf(.rodata) }
    . = data_ld;
    . (NOLOAD) : { bin/ksym_rel.elf(.data) }
    . = bss_ld;
    . (NOLOAD) : { bin/ksym_rel.elf(.bss) }

    . = load_addr;
    . : { bin/module.elf(.text) }
    . : { bin/module.elf(.rodata) }
    . : { bin/module.elf(.data) }
    . : { bin/module.elf(.bss) }
}