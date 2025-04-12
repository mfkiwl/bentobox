#include <kernel/vfs.h>
#include <kernel/mmu.h>
#include <kernel/panic.h>
#include <kernel/ctype.h>
#include <kernel/printf.h>
#include <kernel/string.h>

#include <kernel/arch/x86_64/vmm.h>

uint64_t hex_to_long(const char *str) {
    uint64_t result = 0;
    while (*str) {
        char c = *str++;
        result = (result << 4) + (isdigit(c) ? (c - '0') : (tolower(c) - 'a' + 10));
    }
    return result;
}

void main(void) {
    char input[128] = {0};
    for (;;) {
        fprintf(stdout, ">>> ");
        fgets(input, sizeof(input), stdin);

        if (!input[0]) {}
        else if (!strncmp(input, "list ", 5)) {
            uint64_t addr = hex_to_long(input + 5);
            int i, len = 16;

            for (i = 0; i < len; i++) {
                fprintf(stdout, "0x%x ", *(uint64_t *)(addr));
                if ((i % 4) == 3) {
                    fprintf(stdout, "\n");
                }
                addr += 4;
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
            extern uint64_t pmm_usable_mem;
            fprintf(stdout, "Usable memory: %luK\n", pmm_usable_mem / 1024);
        } else {
            fprintf(stdout, "%s: command not found\n", input);
        }

        memset(input, 0, sizeof(input));
    }
}