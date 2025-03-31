#include <stdarg.h>
#include <kernel/printf.h>

extern void generic_fatal(void);

void __panic(char *file, int line, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024] = {-1};
    vsprintf(buf, fmt, args);
    va_end(args);
    printf("%s:%d: Kernel panic: %s\n", file, line, buf);
    generic_fatal();
}