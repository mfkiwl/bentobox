#include <kernel/arch/x86_64/vga.h>
#include <kernel/mmu.h>
#include <kernel/lfb.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/multiboot.h>
#include <flanterm.h>
#include <backends/fb.h>

struct framebuffer lfb;
struct flanterm_context *ft_ctx;

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

void lfb_initialize(void *mboot_info) {
    struct multiboot_tag_framebuffer *fb = mboot2_find_tag(mboot_info, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);

    if (!fb || fb->common.framebuffer_addr == 0xB8000) {
        dprintf("%s:%d: framebuffer not found, falling back to VGA text mode...\n", __FILE__, __LINE__);
        vga_clear();
        vga_enable_cursor();
        return;
    }
    dprintf("%s:%d: found framebuffer at 0x%lx\n", __FILE__, __LINE__, fb->common.framebuffer_addr);

    mmu_map_pages((ALIGN_UP((fb->common.framebuffer_pitch * fb->common.framebuffer_height), PAGE_SIZE) / PAGE_SIZE), (uintptr_t)fb->common.framebuffer_addr, (uintptr_t)fb->common.framebuffer_addr, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    memset((void *)fb->common.framebuffer_addr, 0x00, fb->common.framebuffer_pitch * fb->common.framebuffer_height);

    lfb.addr = fb->common.framebuffer_addr;
    lfb.width = fb->common.framebuffer_width;
    lfb.height = fb->common.framebuffer_height;
    lfb.pitch = fb->common.framebuffer_pitch;

    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        (uint32_t *)fb->common.framebuffer_addr,
        fb->common.framebuffer_width,
        fb->common.framebuffer_height,
        fb->common.framebuffer_pitch,
        fb->framebuffer_red_mask_size,
        fb->framebuffer_red_field_position,
        fb->framebuffer_green_mask_size,
        fb->framebuffer_green_field_position,
        fb->framebuffer_blue_mask_size,
        fb->framebuffer_blue_field_position,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0
    );
}