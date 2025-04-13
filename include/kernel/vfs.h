#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_PATH 256

typedef enum vfs_node_type {
    NONE,
    VFS_FILE,
    VFS_DIRECTORY,
    VFS_CHARDEVICE,
    VFS_BLOCKDEVICE
} vfs_node_type_t;

typedef struct vfs_node {
    char name[MAX_PATH];
    bool open;
    enum vfs_node_type type;
    uint32_t size;
    uint32_t perms;
    struct vfs_node *parent;
    struct vfs_node *children;
    struct vfs_node *next;
    int32_t(*read)(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len);
    int32_t(*write)(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len);
} vfs_node_t;

extern struct vfs_node *vfs_root;

void  vfs_install(void);
void  vfs_add_node(struct vfs_node *parent, struct vfs_node *node);
void  vfs_add_device(struct vfs_node *node);
char *vfs_get_path(struct vfs_node *node);
int32_t vfs_read(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len);
int32_t vfs_write(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len);
struct vfs_node *vfs_create_node(const char *name, enum vfs_node_type type);
struct vfs_node *vfs_open(struct vfs_node *current, const char *path);