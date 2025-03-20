#include <stdint.h>
#include <misc/printf.h>

extern void generic_fatal(void);

void __assert_failed(const char *file, uint32_t line, const char *func, const char *cond) {
    printf("%s:%d (%s) Assertion failed: %s\n", file, line, func, cond);
    generic_fatal();
}