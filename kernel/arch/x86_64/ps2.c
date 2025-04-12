#include <stdbool.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/ps2.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/fifo.h>
#include <kernel/printf.h>
#include <kernel/string.h>

bool kb_caps = false;
bool kb_ctrl = false;
bool kb_shift = false;
struct fifo kb_fifo;

void irq1_handler(struct registers *r) {
    uint8_t key = inb(0x60);
    if (!(key & 0x80)) {
        switch (key) {
            case 0x2a:
                kb_shift = true;
                break;
            case 0x36:
                kb_shift = true;
                break;
            case 0x1d:
                kb_ctrl = true;
                break;
            case 0x3a:
                kb_caps = !kb_caps;
                break;
            default:
                if (key >= sizeof(kb_map_keys)) {
                    break;
                }
                if (kb_shift) {
                    fifo_enqueue(&kb_fifo, kb_map_keys_shift[key]);
                } else if (kb_caps) {
                    fifo_enqueue(&kb_fifo, kb_map_keys_caps[key]);
                } else {
                    fifo_enqueue(&kb_fifo, kb_map_keys[key]);
                }
                break;
        }
    } else {
        switch (key) {
            case 0xaa:
                kb_shift = false;
                break;
            case 0xb6:
                kb_shift = false;
                break;
            case 0x9d:
                kb_ctrl = false;
                break;
        }
    }
    lapic_eoi();
}

int32_t ps2_keyboard_read(struct vfs_node *node, void *buffer, uint32_t len) {
    int c = 0;
    while (!fifo_dequeue(&kb_fifo, &c)) {}

    memcpy(buffer, &c, 1);
    return 1;
}

void ps2_dev_install(void) {
    struct vfs_node *keyboard = vfs_create_node("keyboard", VFS_CHARDEVICE);
    keyboard->read = ps2_keyboard_read;
    vfs_add_node(vfs_dev, keyboard);
}

void ps2_install(void) {
    fifo_init(&kb_fifo, 16);
    irq_register(1, irq1_handler);
}