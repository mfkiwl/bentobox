#include <stddef.h>

int strncmp(const char *x, const char *y, register size_t n) {
    unsigned char u1, u2;

    while (n-- > 0) {
        u1 = (unsigned char) *x++;
        u2 = (unsigned char) *y++;
        if (u1 != u2)
	        return u1 - u2;
        if (u1 == '\0')
	        return 0;
    }
    return 0;
}