#include <limine.h>
#include <stdbool.h>
#include <kernel/mmu.h>
#include <kernel/elf64.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/module.h>

Elf64_Sym *ksymtab = NULL;
char *kstrtab = NULL;
int ksym_count = 0;

extern void generic_map_kernel(uintptr_t *pml4);

Elf64_Addr elf_symbol_addr(Elf64_Sym *symtab, const char *strtab, int symbol_count, char *str, bool cast) {
    Elf64_Addr offset = 0;

    char *off = strchr(str, '+');
    if (off) {
        *off = '\0';
        offset = atoi(off + 1);
    }

    for (int i = 0; i < symbol_count; i++) {
        if (!strcmp(&strtab[symtab[i].st_name], str)) {
            if (cast) return *(Elf64_Addr *)(symtab[i].st_value) + offset;
            else return symtab[i].st_value + offset;
        }
    }
    return 0;
}

int elf_symbol_name(char *s, Elf64_Sym *symtab, const char *strtab, int symbol_count, Elf64_Addr addr) {
    if (!symtab || !strtab || !symbol_count) {
        strcpy(s, "(none)");
        return 1;
    }

    Elf64_Sym *sym = NULL;
    Elf64_Addr best = (Elf64_Addr)-1;
    
    for (int i = 0; i < symbol_count; i++) {
        if (symtab[i].st_value == 0) {
            continue;
        }
        if (symtab[i].st_value <= addr) {
            if (symtab[i].st_size > 0 && addr < symtab[i].st_value + symtab[i].st_size) {
                sym = &symtab[i];
            }
            
            Elf64_Addr distance = addr - symtab[i].st_value;
            if (distance < best) {
                best = distance;
                sym = &symtab[i];
            }
        }
    }

    if (!sym || (symtab == ksymtab && !strcmp(&strtab[sym->st_name], "end"))) {
        strcpy(s, "(none)");
        return 1;
    }

    sprintf(s, "%s+%lu", &strtab[sym->st_name], addr - sym->st_value);
    return 0;
}

int elf_module(struct limine_file *file) {
    dprintf("%s:%d: loading module \"%s\"\n", __FILE__, __LINE__, file->cmdline);

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(uintptr_t)file->address;

    if (memcmp(ehdr->e_ident, "\x7f""ELF", 4)) {
        printf("%s:%d: invalid elf file\n", __FILE__, __LINE__);
        return -1;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        printf("%s:%d: unsupported elf class\n", __FILE__, __LINE__);
        return -1;
    }

    Elf64_Shdr *shdr = (Elf64_Shdr *)(file->address + ehdr->e_shoff);
    Elf64_Sym *symtab = NULL;
    char *strtab = NULL;

    int i, symbol_count = 0;
    for (i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_STRTAB && ehdr->e_shstrndx != i) {
            strtab = (char *)(file->address + shdr[i].sh_offset);
        } else if (shdr[i].sh_type == SHT_SYMTAB) {
            symtab = (Elf64_Sym *)(file->address + shdr[i].sh_offset);
            symbol_count = shdr[i].sh_size / shdr[i].sh_entsize;
        }
    }

    if (!strcmp(file->cmdline, "ksym.elf")) {
        ksymtab = symtab;
        kstrtab = strtab;
        ksym_count = symbol_count;
        return 0;
    }

    struct Module *metadata = (struct Module *)elf_symbol_addr(symtab, strtab, symbol_count, "metadata", false);
    if (!metadata) {
        printf("%s:%d: Module metadata not found for \"%s\"\n", __FILE__, __LINE__, file->cmdline);
        return -1;
    }

    Elf64_Phdr *phdr = (Elf64_Phdr *)(file->address + ehdr->e_phoff);

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (phdr[i].p_vaddr >= KERNEL_VIRT_BASE)
                continue;

            //dprintf("elf: p_vaddr=0x%lx, p_filesz=0x%lx, p_memsz=0x%lx\n", phdr[i].p_vaddr, phdr[i].p_filesz, phdr[i].p_memsz);

            size_t pages = ALIGN_UP(phdr[i].p_memsz, PAGE_SIZE) / PAGE_SIZE;

            for (size_t page = 0; page < pages; page++) {
                uintptr_t paddr = (uintptr_t)mmu_alloc(1);
                uintptr_t vaddr = phdr[i].p_vaddr + page * PAGE_SIZE;

                mmu_map(vaddr, paddr, PTE_PRESENT | PTE_WRITABLE);
            }

            if (phdr[i].p_filesz > 0) {
                uintptr_t src = (uintptr_t)file->address + phdr[i].p_offset;
                uintptr_t dest = phdr[i].p_vaddr;

                memcpy((void *)dest, (void *)src, phdr[i].p_filesz);
            }

            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((void *)(phdr[i].p_vaddr + phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    return metadata->init();
}