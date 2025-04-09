#include <kernel/vfs.h>
#include <kernel/panic.h>
#include <kernel/ctype.h>
#include <kernel/printf.h>
#include <kernel/string.h>

uint64_t hex_to_long(const char *str) {
    uint64_t result = 0;
    while (*str) {
        char c = *str++;
        result = (result << 4) + (isdigit(c) ? (c - '0') : (tolower(c) - 'a' + 10));
    }
    return result;
}

void debugger_task_entry(void) {
    char input[128];
    for (;;) {
        dprintf(">>> ");

        vfs_read(stdin, input, sizeof(input));
        if (!strncmp(input, "list ", 5)) {
            uint64_t addr = hex_to_long(input + 5);
            int i, len = 16;

            for (i = 0; i < len; i++) {
                dprintf("0x%x ", *(uint64_t *)(addr));
                if ((i % 4) == 3) {
                    dprintf("\n");
                }
                addr += 4;
            }
            continue;
        }
        if (!strncmp(input, "int3", 5)) {
#ifdef __x86_64__
            asm volatile ("int3");
#else
            dprintf("Not supported\n");
            continue;
#endif
        }
        if (!strncmp(input, "panic ", 6)) {
            panic(input + 6);
            continue;
        }
        if (!strncmp(input, "cls", 4) || !strncmp(input, "clear", 6)) {
            dprintf("\033[2J\033[H");
            continue;
        }
        if (!strncmp(input, "help", 5)) {
            dprintf("bentobox debugger (%s)\n", stdout->name);
            dprintf("Built-in commands: list, int3, cls/clear, help\n");
            continue;
        }
    }
}