#include <stddef.h>
#include <libc/syscall.h>

long read(int fd, const void *buf, size_t count) {
    return syscall(0, fd, (long)buf, count);
}