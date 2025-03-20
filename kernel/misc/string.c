#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
	asm volatile("rep movsb"
	            : : "D"(dest), "S"(src), "c"(n)
	            : "flags", "memory");
	return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

int strlen(const char* s) {
    int i = 0;
    while (*s != '\0') {
        i++;
        s++;
    }
    return i;
}

bool strcmp(const char* a, const char* b) {
    if (strlen(a) != strlen(b)) return 1;
    for (int i = 0; i < strlen(a); i++) {
        if (a[i] != b[i]) return 1;
    }
    return 0;
}

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

char *strcpy(char* dest, const char* src) {
    if (dest == NULL) {
        return NULL;
    }
 
    char *ptr = dest;
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
 
    *dest = '\0';
    return ptr;
}

char *strchr(register const char *s, int c) {
    do {
        if (*s == c) {
            return (char *)s;
        }
    } while (*s++);
    return 0;
}

char *strstr(const char *a, const char *b) {
    const size_t len = strlen(b);

    if (!len) return (char *)a;

    for (const char *p = a; (p = strchr(p, *b)) != 0; p++) {
        if (strncmp(p, b, len) == 0) {
            return (char *)p;
        }
    }
    return 0;
}

int atoi(char *s) {
    int acum = 0;
    while((*s >= '0') && (*s <= '9')) {
        acum = acum * 10;
        acum = acum + (*s - 48);
        s++;
    }
    return acum;
}