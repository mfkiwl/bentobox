#include <errno.h>
#include <kernel/elf64.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/multiboot.h>

int elf_module(struct multiboot_tag_module *mod) {
    printf("elf: loading module \"%s\"\n", mod->string);

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(uintptr_t)mod->mod_start;

    if (memcmp(ehdr->e_ident, "\x7f""ELF", 4)) {
        printf("elf: invalid elf file\n");
        return -EINVAL;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        printf("elf: unsupported elf class\n");
        return -EINVAL;
    }

    Elf64_Phdr *phdr = (Elf64_Phdr *)(mod->mod_start + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        switch (phdr[i].p_type) {
            case PT_LOAD:
                // TODO: load segment
                continue;
            default:
                continue;
        }
    }

    return 0;
}