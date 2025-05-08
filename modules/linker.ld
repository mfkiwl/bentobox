PHDRS
{
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = text_ld;
    . (NOLOAD) : { bin/ksym_rel.elf(.text) }
    . = rodata_ld;
    . (NOLOAD) : { bin/ksym_rel.elf(.rodata) }
    . = data_ld;
    . (NOLOAD) : { bin/ksym_rel.elf(.data) }
    . = bss_ld;
    . (NOLOAD) : {
        bin/ksym_rel.elf(COMMON)
        bin/ksym_rel.elf(.bss)
    }

    . = load_addr;
    . : { bin/module.elf(.text) } :text
    . : { bin/module.elf(.rodata) } :rodata
    . : { bin/module.elf(.data) } :data
    . : { bin/module.elf(.bss) } :data
}