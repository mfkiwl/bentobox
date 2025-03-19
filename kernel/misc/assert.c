#include <stdint.h>
#include <misc/printf.h>

extern void arch_fatal(void);

void __assert_failed(const char *file, uint32_t line, const char *func, const char *cond) {
    dprintf("%s:%d (%s) Assertion failed: %s\n", file, line, func, cond);
    arch_fatal();
}