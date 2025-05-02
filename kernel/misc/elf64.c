#include "kernel/arch/x86_64/smp.h"
#include "kernel/arch/x86_64/vmm.h"
#include "kernel/sched.h"
#include <stdbool.h>
#include <kernel/mmu.h>
#include <kernel/elf64.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/module.h>
#include <kernel/multiboot.h>

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

int elf_module(struct multiboot_tag_module *mod) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(uintptr_t)mod->mod_start;

    if (memcmp(ehdr->e_ident, "\x7f""ELF", 4)) {
        printf("%s:%d: invalid elf file\n", __FILE__, __LINE__);
        return -1;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        printf("%s:%d: unsupported elf class\n", __FILE__, __LINE__);
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

    struct Module *metadata = (struct Module *)elf_symbol_addr(symtab, strtab, symbol_count, "metadata", false);
    if (!metadata) {
        printf("%s:%d: Module metadata not found for \"%s\"\n", __FILE__, __LINE__, mod->string);
        return -1;
    }

    Elf64_Phdr *phdr = (Elf64_Phdr *)(mod->mod_start + ehdr->e_phoff);

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (phdr[i].p_filesz == 0 && phdr[i].p_memsz > 0)
                continue;

            size_t pages = ALIGN_UP(phdr[i].p_memsz, PAGE_SIZE) / PAGE_SIZE;

            for (size_t page = 0; page < pages; page++) {
                uintptr_t vaddr = phdr[i].p_vaddr + page * PAGE_SIZE;

                mmu_mark_used((void *)vaddr, 1);
            }

            if (phdr[i].p_filesz > 0) {
                uintptr_t src = (uintptr_t)mod->mod_start + phdr[i].p_offset;
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

int elf_exec(const char *file) {
    struct vfs_node *fptr = vfs_open(NULL, file);
    if (!fptr) {
        printf("%s:%d: cannot open file \"%s\"\n", __FILE__, __LINE__, file);
        return -1;
    }

    void *buffer = kmalloc(fptr->size);
    vfs_read(fptr, buffer, 0, fptr->size);
    
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buffer;

    if (memcmp(ehdr->e_ident, "\x7f""ELF", 4)) {
        printf("%s:%d: invalid elf file\n", __FILE__, __LINE__);
        kfree(buffer);
        return -1;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        printf("%s:%d: unsupported elf class\n", __FILE__, __LINE__);
        kfree(buffer);
        return -1;
    }

    sched_stop_timer();
    struct task *proc = sched_new_user_task((void *)ehdr->e_entry, "elf64", -1);
    vmm_switch_pm(proc->pml4);
    dprintf("Mapping sections!!\n");
    
    Elf64_Phdr *phdr = (Elf64_Phdr *)((uintptr_t)buffer + ehdr->e_phoff);

    int i, section = 0;
    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (phdr[i].p_filesz == 0 && phdr[i].p_memsz > 0)
                continue;

            size_t pages = ALIGN_UP(phdr[i].p_memsz, PAGE_SIZE) / PAGE_SIZE;

            for (size_t page = 0; page < pages; page++) {
                uintptr_t paddr = (uintptr_t)mmu_alloc(1);
                uintptr_t vaddr = phdr[i].p_vaddr + page * PAGE_SIZE;

                mmu_map(vaddr, paddr, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
            }

            proc->sections[section].ptr = phdr[i].p_vaddr;
            proc->sections[section].length = pages * PAGE_SIZE;
            section++;

            if (phdr[i].p_filesz > 0) {
                uintptr_t src = (uintptr_t)(uintptr_t)buffer + phdr[i].p_offset;
                uintptr_t dest = phdr[i].p_vaddr;

                memcpy((void *)dest, (void *)src, phdr[i].p_filesz);
            }

            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((void *)(phdr[i].p_vaddr + phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    vmm_switch_pm(kernel_pd);
    kfree(buffer);
    sched_yield();
    return 0;
}