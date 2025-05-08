#pragma once
#include <kernel/vfs.h>

#define stdin  0
#define stdout 1
#define stderr 2

struct fd {
    struct vfs_node *node;
    uint16_t flags;
};

struct fd fd_open(struct vfs_node *node, uint16_t flags);