#include <limine.h>
#include <kernel/arch/x86_64/vga.h>
#include <kernel/mmu.h>
#include <kernel/lfb.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/assert.h>
#include <kernel/flanterm.h>

struct framebuffer lfb;
struct flanterm_context *ft_ctx;

struct limine_framebuffer_request fb = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

void fb_draw_char(struct framebuffer *fb, uint32_t x, uint32_t y, uint8_t c, uint32_t fore, uint32_t back) {
    uint32_t *display = (uint32_t *)fb->addr;
    for (int i = 0; i < 16; i++) {
        for (int o = 0; o < 8; o++) {
            if (builtin_font[c * 16 + i] & (1 << (7 - o))) {
                display[(y + i) * fb->width + x + o + (x / 8)] = fore;
            } else {
                display[(y + i) * fb->width + x + o + (x / 8)] = back;
            }
        }
    }
}

void lfb_initialize(void) {
#ifdef __x86_64__
    if (fb.response == NULL || fb.response->framebuffer_count < 1) {
        dprintf("%s:%d: No framebuffers!\n");
        for (;;);
    }

    dprintf("%s:%d: found framebuffer at 0x%lx\n", __FILE__, __LINE__, fb.response->framebuffers[0]->address);

    lfb.addr = (uint64_t)fb.response->framebuffers[0]->address;
    lfb.width = fb.response->framebuffers[0]->width;
    lfb.height = fb.response->framebuffers[0]->height;
    lfb.pitch = fb.response->framebuffers[0]->pitch;

    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        (uint32_t *)fb.response->framebuffers[0]->address,
        fb.response->framebuffers[0]->width,
        fb.response->framebuffers[0]->height,
        fb.response->framebuffers[0]->pitch,
        fb.response->framebuffers[0]->red_mask_size,
        fb.response->framebuffers[0]->red_mask_shift,
        fb.response->framebuffers[0]->green_mask_size,
        fb.response->framebuffers[0]->green_mask_shift,
        fb.response->framebuffers[0]->blue_mask_size,
        fb.response->framebuffers[0]->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0
    );
#else
    unimplemented;
#endif
}