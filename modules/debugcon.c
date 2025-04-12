#include <kernel/vfs.h>
#include <kernel/mmu.h>
#include <kernel/ksym.h>
#include <kernel/panic.h>
#include <kernel/ctype.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/module.h>

uint64_t hex_to_long(const char *str) {
    uint64_t result = 0;
    while (*str) {
        char c = *str++;
        result = (result << 4) + (isdigit(c) ? (c - '0') : (tolower(c) - 'a' + 10));
    }
    return result;
}

int init() {
    char input[128] = {0};
    for (;;) {
        fprintf(stdout, ">>> ");
        fgets(input, sizeof(input), stdin);

        if (!input[0]) {}
        else if (!strncmp(input, "list ", 5)) {
            uint64_t addr = !strncmp(input + 5, "0x", 2) ? hex_to_long(input + 7) : elf_symbol_addr(ksymtab, kstrtab, ksym_count, input + 5);
            if (!addr) {
                fprintf(stdout, "Symbol not found\n");
            } else {
                int i, len = 16;

                for (i = 0; i < len; i++) {
                    fprintf(stdout, "0x%x ", *(uint64_t *)(addr));
                    if ((i % 4) == 3) {
                        fprintf(stdout, "\n");
                    }
                    addr += 4;
                }
            }
        } else if (!strncmp(input, "int3", 5)) {
#ifdef __x86_64__
            asm volatile ("int3");
#else
            fprintf(stdout, "Not supported\n");
#endif
        } else if (!strncmp(input, "panic ", 6)) {
            panic(input + 6);
        } else if (!strncmp(input, "cls", 4) || !strncmp(input, "clear", 6)) {
            fprintf(stdout, "\033[2J\033[H");
        } else if (!strncmp(input, "help", 5)) {
            fprintf(stdout, "bentobox debugger (%s)\n", this_core()->current_proc->fd_table[1]->name);
            fprintf(stdout, "Built-in commands: list, int3, cls/clear, help, mem\n");
        } else if (!strncmp(input, "mem", 4)) {
            fprintf(stdout, "Total memory: %luK\n", mmu_usable_mem / 1024);
            fprintf(stdout, "Used memory: %luK\n", mmu_used_pages * 4);
            fprintf(stdout, "Free memory: %luK\n", mmu_usable_mem / 1024 - mmu_used_pages * 4);
        } else {
            fprintf(stdout, "%s: command not found\n", input);
        }

        memset(input, 0, sizeof(input));
    }
}

struct Module metadata = {
    .name = "bentobox debug shell",
    .init = init
};