#include <kernel/arch/x86_64/vga.h>
#include <kernel/mmu.h>
#include <kernel/lfb.h>
#include <kernel/string.h>
#include <kernel/printf.h>
#include <kernel/assert.h>
#include <kernel/flanterm.h>
#include <kernel/multiboot.h>

struct framebuffer lfb;
struct flanterm_context *ft_ctx;

void lfb_initialize(void) {
#ifdef __x86_64__
    extern void *mboot;
    struct multiboot_tag_framebuffer *fb = mboot2_find_tag(mboot, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);

    if (!fb || fb->common.framebuffer_addr == 0xB8000) {
        dprintf("%s:%d: framebuffer not found, falling back to VGA text mode...\n", __FILE__, __LINE__);
        vga_clear();
        vga_enable_cursor();
        return;
    }
    dprintf("%s:%d: found framebuffer at 0x%p\n", __FILE__, __LINE__, fb->common.framebuffer_addr);

    mmu_map_pages((ALIGN_UP((fb->common.framebuffer_pitch * fb->common.framebuffer_height), PAGE_SIZE) / PAGE_SIZE), VIRTUAL(fb->common.framebuffer_addr), (void *)fb->common.framebuffer_addr, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    //memset((void *)fb->common.framebuffer_addr, 0x00, fb->common.framebuffer_pitch * fb->common.framebuffer_height);

    lfb.addr = (uint64_t)VIRTUAL(fb->common.framebuffer_addr);
    lfb.width = fb->common.framebuffer_width;
    lfb.height = fb->common.framebuffer_height;
    lfb.pitch = fb->common.framebuffer_pitch;

    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        (uint32_t *)lfb.addr,
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
#else
    unimplemented;
#endif
}