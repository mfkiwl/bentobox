#pragma once
#include <stdint.h>
#include <stdatomic.h>
#include <kernel/flanterm.h>
#include <kernel/multiboot.h>

struct framebuffer {
    uint64_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    struct multiboot_tag_framebuffer *fb;
};

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

extern struct framebuffer lfb;
extern struct flanterm_context *ft_ctx;

void fb_draw_char(struct framebuffer *fb, uint32_t x, uint32_t y, uint8_t c, uint32_t fore, uint32_t back);
void lfb_initialize(void);
void lfb_get_ws(struct winsize *ws);