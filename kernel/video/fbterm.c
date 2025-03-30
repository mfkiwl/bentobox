#include <kernel/lfb.h>
#include <kernel/string.h>
#include <kernel/fbterm.h>
#include <kernel/printf.h>

int ansi_to_rgb(int ansi) {
    uint32_t table[] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0xFF5555, 0x55FF55, 0xFFFF55,
        0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF
    };

    if (ansi == 0) {
        return 0xAAAAAA;
    } else if (ansi >= 30 && ansi <= 37) {
        return table[ansi - 30];
    } else if (ansi >= 40 && ansi <= 47) {
        return table[ansi - 40];
    } else if (ansi >= 90 && ansi <= 97) {
        return table[ansi - 82];
    } else if (ansi >= 100 && ansi <= 107) {
        return table[ansi - 92];
    } else {
        return 0x000000;
    }
}

void fbterm_draw_cursor(struct console *console, int inverted) {
    uint32_t *display = (uint32_t *)console->fb->addr;
    
    for (int i = 16 - CURSOR_HEIGHT; i < 16; i++) {
        for (int o = 0; o < 8; o++) {
            display[((console->y * 16) + i + console->border_y) *
                console->fb->width + (console->x * 8 + console->border_x) + o + console->x] =
                inverted ? 0x000000 : ansi_to_rgb(console->foreground);
        }
    }
}

void fbterm_scroll(struct console *console) {
    memcpy((uint8_t *)console->fb->addr + (console->fb->pitch * console->border_y), (uint8_t *)console->fb->addr + ((16 + console->border_y) * console->fb->pitch), (console->fb->height - 16) * console->fb->pitch);
    memset((uint8_t *)console->fb->addr + (console->fb->height - 16 - console->border_y) * console->fb->pitch, 0x00, 16 * console->fb->pitch);

    console->y--;
}

void fbterm_putchar(struct console *console, uint8_t c) {
    if (console->ansi_code[0] == '\033') {
        switch (c) {
            case '[':
                console->ansi_code[1] = '[';
                console->ansi_index = 2;
                return;
            case 'm':
                console->ansi_code[console->ansi_index] = 0;
                console->ansi_index = 0;
                int code = atoi(console->ansi_code + 2);
                if (code == 0) {
                    console->foreground = 37;
                    console->background = 40;
                } else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97)) {
                    console->foreground = code;
                } else if ((code >= 40 && code <= 47) || (code >= 100 && code <= 107)) {
                    console->background = code;
                }
                memset(console->ansi_code, 0, sizeof(console->ansi_code));
                return;
            default:
                console->ansi_code[console->ansi_index++] = c;
                return;
        }
    }

    switch (c) {
        case '\033':
            console->ansi_code[0] = '\033';
            break;
        case '\n':
            fbterm_draw_cursor(console, 1);
            console->x = console->width;
            break;
        case '\b':
            fbterm_draw_cursor(console, 1);
            if (!console->x) {
                console->x = console->width - 1;
                console->y--;
            } else {
                console->x--;
            }
            fbterm_draw_cursor(console, 0);
            break;
        case '\r':
            fbterm_draw_cursor(console, 1);
            console->x = 0;
            fbterm_draw_cursor(console, 0);
            break;
        case '\t':
            fbterm_puts(console, "    ");
            break;
        default:
            fb_draw_char(console->fb, console->x * 8 + console->border_x, console->y * 16 +
                console->border_y, c, ansi_to_rgb(console->foreground), ansi_to_rgb(console->background));
            console->x++;
            break;
    }

    if (console->x >= console->width) { 
        console->x = 0;
        console->y++;

        if (console->y >= console->height) {
            fbterm_scroll(console);
        }
    }
}

void fbterm_puts(struct console *console, char *str) {
    while (*str) {
        fbterm_putchar(console, (uint8_t)*str++);
    }
    fbterm_draw_cursor(console, 0);
}

void fbterm_init(struct console *console, struct framebuffer *fb) {
    if (!fb->addr)
        return;

    uint32_t border = fb->height % 16 / 2;

    console->width = (fb->width - border) / 8;
    console->height = fb->height / 16;
    console->border_x = border;
    console->border_y = border;
    console->fb = fb;
    console->foreground = 37;
    console->background = 40;
    memset(console->ansi_code, 0, 8);
    console->ansi_index = 0;
}