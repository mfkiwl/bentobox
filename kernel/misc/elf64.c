#include <errno.h>
#include <kernel/mmu.h>
#include <kernel/elf64.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/multiboot.h>

char *elf_get_symbol_name(Elf64 *elf, Elf64_Addr addr) {
    for (int i = 0; i < elf->symbol_count; i++) {
        if (elf->symtab[i].st_value == addr) {
            return &elf->strtab[elf->symtab[i].st_name];
        }
    }
    return "(none)";
}

Elf64 *elf_module(struct multiboot_tag_module *mod) {
    printf("elf: loading module \"%s\"\n", mod->string);

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(uintptr_t)mod->mod_start;

    if (memcmp(ehdr->e_ident, "\x7f""ELF", 4)) {
        printf("elf: invalid elf file\n");
        return NULL;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        printf("elf: unsupported elf class\n");
        return NULL;
    }

    Elf64_Shdr *shdr = (Elf64_Shdr *)(mod->mod_start + ehdr->e_shoff);
    Elf64_Sym *symtab = NULL;
    char *strtab = NULL;
    int i, symbol_count = 0;

    for (i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_STRTAB && ehdr->e_shstrndx != i) {
            strtab = (char *)(mod->mod_start + shdr[i].sh_offset);
        } else if (shdr[i].sh_type == SHT_SYMTAB) {
            symtab = (Elf64_Sym *)(mod->mod_start + shdr[i].sh_offset);
            symbol_count = shdr[i].sh_size / shdr[i].sh_entsize;
        }
    }

    for (i = 0; i < symbol_count; i++) {
        printf("elf: addr=0x%lx, symbol=%s\n", symtab[i].st_value, &strtab[symtab[i].st_name]);
    }

    Elf64_Phdr *phdr = (Elf64_Phdr *)(mod->mod_start + ehdr->e_phoff);

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            // TODO: load segment
        }
    }

    Elf64 *elf = kmalloc(sizeof(Elf64));
    elf->symtab = symtab;
    elf->strtab = strtab;
    elf->symbol_count = symbol_count;

    printf("elf: %s\n", elf_get_symbol_name(elf, 0x401000));

    return NULL;
}