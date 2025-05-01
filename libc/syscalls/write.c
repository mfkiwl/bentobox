#include <stddef.h>
#include <libc/syscall.h>

long write(int fd, const void *buf, size_t count) {
    return syscall(1, fd, (long)buf, count);
}