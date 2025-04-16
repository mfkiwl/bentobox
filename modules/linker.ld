SECTIONS
{
    . = text_ld;
    . (NOLOAD) : { bin/ksym.o(.text) }
    . = rodata_ld;
    . (NOLOAD) : { bin/ksym.o(.rodata) }
    . = data_ld;
    . (NOLOAD) : { bin/ksym.o(.data) }
    . (NOLOAD) : { bin/ksym.o(.dynamic) }
    . (NOLOAD) : { bin/ksym.o(.bss) }

    . = load_addr;
    . : { bin/module.elf(.text) }
    . : { bin/module.elf(.rodata) }
    . : { bin/module.elf(.data) }
    . : { bin/module.elf(.bss) }
}