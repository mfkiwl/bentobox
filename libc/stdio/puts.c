#include <libc/string.h>
#include <libc/syscall.h>

int puts(const char *s) {
    return write(1, s, strlen(s));
}