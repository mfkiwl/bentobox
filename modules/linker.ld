ENTRY(main)

SECTIONS
{
    . = 0x100000;
    . (NOLOAD) :
    {
        bin/ksym_rel.elf(.text)
        bin/ksym_rel.elf(.data)
        bin/ksym_rel.elf(.rodata)
        bin/ksym_rel.elf(.bss)
    }

    . = 0x400000;
    . :
    {
        bin/module.elf(.text)
        bin/module.elf(.data)
        bin/module.elf(.rodata)
        bin/module.elf(.bss)
    }
}