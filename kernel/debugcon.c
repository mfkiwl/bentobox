#include "kernel/arch/x86_64/smp.h"
#include "kernel/sched.h"
#include <kernel/fd.h>
#include <kernel/vfs.h>
#include <kernel/mmu.h>
#include <kernel/ksym.h>
#include <kernel/panic.h>
#include <kernel/ctype.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/module.h>

void debugcon_entry(void) {
    char input[128] = {0};
    for (;;) {
        fprintf(stdout, "# ");
        memset(input, 0, sizeof(input));
        fgets(input, sizeof(input), stdin);
        *strchr(input, '\n') = '\0';

        if (!input[0]) {
            continue;
        } else if (!strncmp(input, "clear", 6)) {
            fprintf(stdout, "\033[H\033[2J");
            continue;
        } else if (!strncmp(input, "ls", 2)) {
            struct vfs_node *dir = vfs_open(NULL, input + 3);
            if (!dir) {
                fprintf(stdout, "ls: cannot access '%s': No such file or directory\n", input + 3);
                continue;
            }

            struct vfs_node *child = dir->children;

            if (!child) {
                fprintf(stdout, "\n");
                continue;
            }
            while (child) {
                if (!strcmp(child->name, ".") || !strcmp(child->name, "..")) {
                    child = child->next;
                    continue;
                }

                char *color = NULL;
                switch (child->type) {
                    case VFS_FILE:
                        color = "\033[91m";
                        break;
                    case VFS_DIRECTORY:
                        color = "\033[94m";
                        break;
                    case VFS_CHARDEVICE:
                    case VFS_BLOCKDEVICE:
                        color = "\033[33m";
                        break;
                    default:
                        color = "";
                        break;
                }
                fprintf(stdout, "%s%s  ", color, child->name);
                child = child->next;
            }
            fprintf(stdout, "\033[0m\n");
            continue;
        } else if (!strncmp(input, "ps", 3)) {
            struct task *i = this;
            do {
                printf("%d %s\n", i->pid, i->name);
                i = i->next;
            } while (i != this);
        } else if (!strncmp(input, "cat ", 4)) {
            struct vfs_node *file = vfs_open(NULL, input + 4);
            if (!file) {
                printf("cat: cannot access '%s': No such file or directory\n", input + 4);
                continue;
            }

            char *buf = kmalloc(file->size);
            int len = vfs_read(file, buf, 0, file->size);

            for (int i = 0; i < len; i++) {
                putchar(buf[i]);
            }
            kfree(buf);
            continue;
        } else if (!strncmp(input, ".", 1)) {
            elf_exec(input + 1);
        } else if (!strncmp(input, "exit", 5)) {
            sched_kill(this, 0);
        } else if (!strncmp(input, "ram", 4)) {
            printf("Total memory: %luK\n", mmu_usable_mem / 1024);
            printf("Used pages: %lu\n", mmu_used_pages);
            printf("Free pages: %lu\n", mmu_page_count - mmu_used_pages);
        } else if (!strncmp(input, "setfont", 7)) {
            extern void lfb_change_font(const char *);
            lfb_change_font(input + 8);
        } else {
            fprintf(stdout, "Unknown command: %s\n", input);
        }
    }
}