#include <libc/syscall.h>

char *fgets(char *buf, unsigned int len, int stream) {
    read(stream, buf, len);
    return buf;
}