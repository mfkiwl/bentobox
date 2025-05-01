#include <libc/syscall.h>

void _exit(int status) {
    syscall(60, 0);
    __builtin_unreachable();
}