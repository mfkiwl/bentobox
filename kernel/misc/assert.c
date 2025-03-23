#include <stdint.h>
#include <kernel/printf.h>

extern void generic_fatal(void);

void __assert_failed(const char *file, uint32_t line, const char *func, const char *cond) {
    printf("%s:%d (%s) Assertion failed: %s\n", file, line, func, cond);
    generic_fatal();
}

void __stub(const char *file, uint32_t line, const char *func) {
    dprintf("%s:%d: %s is a stub\n", file, line, func);
}