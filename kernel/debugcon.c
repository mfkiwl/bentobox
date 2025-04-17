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
        fprintf(stdout, ">>> ");
        memset(input, 0, sizeof(input));
        fgets(input, sizeof(input), stdin);

        if (!input[0]) {
            continue;
        } else if (!strncmp(input, "clear", 6)) {
            printf("\033[2J\033[H");
            continue;
        } else if (!strncmp(input, "ls", 2)) {
            struct vfs_node *dir = vfs_open(NULL, input + 3);
            if (!dir) {
                printf("ls: cannot access '%s': No such file or directory\n", input + 3);
                continue;
            }

            struct vfs_node *child = dir->children;

            if (!child) {
                printf("\n");
                continue;
            }
            while (child) {
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
                printf("%s%s  ", color, child->name);
                child = child->next;
            }
            printf("\033[0m\n");
            continue;
        }
    }
}