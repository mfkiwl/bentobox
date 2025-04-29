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

void test_user_process(void) {
    //for (;;);

    static char msg[] = "Hello, world!\n";
    static const size_t msglen = sizeof(msg) - 1;

    // Write "Hello, world!\n" to STDOUT
    __asm__ __volatile__(
        "mov $1, %%rax\n\t"      // syscall number for write
        "mov $1, %%rdi\n\t"      // file descriptor (STDOUT_FILENO)
        "mov %0, %%rsi\n\t"      // pointer to message
        "mov %1, %%rdx\n\t"      // message length
        "syscall\n\t"
        :
        : "r"(msg), "r"(msglen)
        : "rax", "rdi", "rsi", "rdx"
    );

    // Exit with success
    __asm__ __volatile__(
        "mov $60, %%rax\n\t"     // syscall number for exit
        "mov $0, %%rdi\n\t"      // exit code (EXIT_SUCCESS)
        "syscall\n\t"
        :
        :
        : "rax", "rdi"
    );

    for (;;);
}

void test_kern_process(void) {
    printf("Hello kernel!\n");
    sched_kill(this_core()->current_proc, 0);
}

void debugcon_entry(void) {
    char input[128] = {0};
    for (;;) {
        fprintf(stdout, "# ");
        memset(input, 0, sizeof(input));
        fgets(input, sizeof(input), stdin);

        if (!input[0]) {
            continue;
        } else if (!strncmp(input, "clear", 6)) {
            fprintf(stdout, "\033[2J\033[H");
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
            struct task *i = this_core()->current_proc;
            do {
                printf("%d %s\n", i->pid, i->name);
                i = i->next;
            } while (i != this_core()->current_proc);
        } else if (!strncmp(input, "cat ", 4)) {
            struct vfs_node *file = vfs_open(NULL, input + 4);
            if (!file) {
                printf("cat: cannot access '%s': No such file or directory\n", input + 4);
                continue;
            }

            char *buf = kmalloc(file->size);
            memset(buf, 0, file->size);
            vfs_read(file, buf, 0, file->size);

            fprintf(stdout, "%s", buf);
            kfree(buf);
            continue;
        } else if (!strncmp(input, ".", 1)) {
            elf_exec(input + 1);
        } else if (!strncmp(input, "exit", 5)) {
            break;
        } else if (!strncmp(input, "user", 5)) {
            sched_new_user_task(test_user_process, "Test User Process", -1);
        } else if (!strncmp(input, "test ", 5)) {
            int count = atoi(input + 5);
            for (int i = 0; i < count; i++) {
                printf("%d/%d\n", i + 1, count);
                sched_new_task(test_kern_process, "Test Kernel Process", -1);
                sched_sleep(100000);
            }
        } else if (!strncmp(input, "ram", 4)) {
            printf("Total memory: %lu\n", mmu_usable_mem / 1024);
            printf("Used pages: %lu\n", mmu_used_pages);
            printf("Free pages: %lu\n", mmu_page_count - mmu_used_pages);
        } else {
            fprintf(stdout, "Unknown command: %s\n", input);
        }
    }
}