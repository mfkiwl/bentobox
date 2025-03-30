#pragma once
#include <stdint.h>
#include <kernel/lfb.h>

#define CURSOR_HEIGHT 2

struct console {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
	uint32_t border_x;
	uint32_t border_y;
	uint32_t foreground;
	uint32_t background;

	char ansi_code[8];
	int ansi_index;

    struct framebuffer *fb;
};

void fbterm_init(struct console *console, struct framebuffer *fb);
void fbterm_puts(struct console *console, char *str);