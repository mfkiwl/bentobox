#include <kernel/arch/x86_64/smp.h>
#include <kernel/sched.h>
#include <kernel/printf.h>

int sys_read(struct registers *r) {
    struct fd fd = this_core()->current_proc->fd_table[r->rdi];
    if (!fd.node) {
        return -1;
    }
    if (fd.node->read) {
        size_t i = 0;
        char *str = (char *)r->rsi;
        while (i < r->rdx) {
            vfs_read(fd.node, str + i, 0, 1);

            switch (str[i]) {
                case '\0':
                    break;
                case '\n':
                case '\r':
                    fprintf(stdout, "\n");
                    str[i] = '\0';
                    return i;
                case '\b':
                case 127:
                    if (i > 0) {
                        fprintf(stdout, "\b \b");
                        str[i] = '\0';
                        i--;
                    }
                    break;
                default:
                    fprintf(stdout, "%c", str[i]);
                    i++;
                    break;
            }
        }
        return fd.node->read(fd.node, (void *)r->rsi, 0, r->rdx);
    }
    return 0;
}

int sys_write(struct registers *r) {
    struct fd fd = this_core()->current_proc->fd_table[r->rdi];
    if (!fd.node) {
        return -1;
    }
    if (fd.node->write) {
        return fd.node->write(fd.node, (void *)r->rsi, 0, r->rdx);
    }
    return 0;
}