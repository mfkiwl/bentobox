#include <stdarg.h>
#include <stdint.h>
#include <kernel/printf.h>

#define UART_BASE 0x10000000

static uint32_t mmio_read(uintptr_t addr) {
	uint32_t res = *((volatile uint32_t *)(addr));
	return res;
}

static void mmio_write(uintptr_t addr, uint32_t val) {
	*((volatile uint32_t *)(addr)) = val;
}

void uart_putchar(char c) {
    if (c == '\n') {
        mmio_write(UART_BASE, (uint32_t)'\r');
    }
	mmio_write(UART_BASE, (uint32_t)c);
	return;
}

void uart_puts(const char * str) {
	while(*str) {
        uart_putchar(*str++);
    }
}

int dprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024] = {0};
    int ret = vsprintf(buf, fmt, args);
    uart_puts(buf);
    va_end(args);

    return ret;
}