#include <stdarg.h>
#include <stdint.h>
#include <misc/string.h>
#include <video/vga.h>

void parse_num(char *s, int *ptr, uint32_t val, uint32_t base) {
    uint32_t n = val / base;
    int r = val % base;
    if (r < 0) {
        r += base;
        n--;
    }
    if (val >= base) {
        parse_num(s, ptr, n, base);
    }
    s[(*ptr)++] = (r + '0');
}

void parse_hex(char *s, int *ptr, uint32_t val) {
    int i = 8;
    while (i-- > 0) {
        s[(*ptr)++] = "0123456789abcdef"[val >> (i * 4) & 0x0F];
    }
}

void parse_string(char *s, int *ptr, char *str) {
    while (*str) {
        s[(*ptr)++] = *str++;
    }
}

int vsprintf(char *s, const char *fmt, va_list args) {
    int ptr = 0;

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;

            switch (*fmt) {
                case 'd':
                    parse_num(s, &ptr, va_arg(args, int), 10);
                    break;
                case 'x':
                    parse_hex(s, &ptr, va_arg(args, uint32_t));
                    break;
                case 's':
                    parse_string(s, &ptr, va_arg(args, char *));
                    break;
                case 'c':
                    s[ptr++] = (char)va_arg(args, int);
                    break;
            }
        } else {
            s[ptr++] = *fmt;
        }
        fmt++;
    }

    return 0;
}

int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024] = {0};
    int ret = vsprintf(buf, fmt, args);
    vga_puts(buf);
    va_end(args);

    return ret;
}