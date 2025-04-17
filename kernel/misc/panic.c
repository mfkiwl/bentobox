#include <stdarg.h>
#include <kernel/ksym.h>
#include <kernel/elf64.h>
#include <kernel/sched.h>
#include <kernel/printf.h>

extern void generic_fatal(void);

void __panic(char *file, int line, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024] = {-1};
    vsprintf(buf, fmt, args);
    va_end(args);

    printf("%s:%d: Kernel panic: %s\n", file, line, buf);

    struct stackframe *frame_ptr = __builtin_frame_address(0);

    printf("%s:%d: traceback:\n", __FILE__, __LINE__);

    char symbol[256];
    struct cpu *this = this_core();
    for (int i = 0; i < 8 && frame_ptr->rbp; i++) {
        if (elf_symbol_name(symbol, ksymtab, kstrtab, ksym_count, frame_ptr->rip)) {
            elf_symbol_name(symbol,
                this->current_proc->elf.symtab, this->current_proc->elf.strtab,
                this->current_proc->elf.symbol_count, frame_ptr->rip
            );
        }
        printf("#%d  0x%p in %s\n", i, frame_ptr->rip, symbol);
        frame_ptr = frame_ptr->rbp;
    }

    generic_fatal();
}