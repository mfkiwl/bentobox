#include <kernel/mmu.h>
#include <kernel/elf64.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/multiboot.h>

Elf64_Sym *ksymtab = NULL;
char *kstrtab = NULL;
int ksym_count = 0;

int elf_symbol_name(char *s, Elf64_Sym *symtab, const char *strtab, int symbol_count, Elf64_Addr addr) {
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

    if (symtab == ksymtab && !strcmp(&strtab[sym->st_name], "end")) {
        strcpy(s, "(none)");
        return 1;
    }

    if (!sym) {
        strcpy(s, "(none)");
        return 1;
    }

    sprintf(s, "%s+%lu", &strtab[sym->st_name], addr - sym->st_value);
    return 0;
}

int elf_module(struct multiboot_tag_module *mod) {
    dprintf("%s:%d: loading module \"%s\"\n", __FILE__, __LINE__, mod->string);

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(uintptr_t)mod->mod_start;

    if (memcmp(ehdr->e_ident, "\x7f""ELF", 4)) {
        printf("elf: invalid elf file\n");
        return -1;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        printf("elf: unsupported elf class\n");
        return -1;
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

    if (!strcmp(mod->string, "ksym")) {
        ksymtab = symtab;
        kstrtab = strtab;
        ksym_count = symbol_count;
        return 0;
    }

    Elf64_Phdr *phdr = (Elf64_Phdr *)(mod->mod_start + ehdr->e_phoff);

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (phdr[i].p_filesz == 0 && phdr[i].p_memsz > 0)
                continue;

            printf("elf: loading segment: vaddr=0x%lx, size=0x%lx\n", phdr[i].p_vaddr, phdr[i].p_memsz);

            size_t pages = ALIGN_UP(phdr[i].p_memsz, PAGE_SIZE) / PAGE_SIZE;
            uintptr_t paddr = (uintptr_t)mmu_alloc(pages);
            uintptr_t vaddr = phdr[i].p_vaddr;

            mmu_map_pages(pages, paddr, vaddr, PTE_PRESENT | PTE_WRITABLE);

            if (phdr[i].p_filesz > 0) {
                uintptr_t src = (uintptr_t)mod->mod_start + phdr[i].p_offset;
                uintptr_t dest = paddr;

                memcpy((void *)dest, (void *)src, phdr[i].p_filesz);
            }

            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((void *)(paddr + phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    struct task *proc = sched_new_task((void *)ehdr->e_entry, mod->string, -1);
    proc->elf.symtab = symtab;
    proc->elf.strtab = strtab;
    proc->elf.symbol_count = symbol_count;

    return 0;
}