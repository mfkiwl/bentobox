#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <kernel/vfs.h>
#include <kernel/mmu.h>
#include <kernel/malloc.h>
#include <kernel/string.h>
#include <kernel/printf.h>

extern void zero_initialize(void);
extern void ps2_initialize(void);
extern void serial_initialize(void);
extern void console_initialize(void);

struct vfs_node *vfs_root = NULL;
struct vfs_node *vfs_dev = NULL;

const char *vfs_types[] = {
    "VFS_NONE",
    "VFS_FILE",
    "VFS_DIRECTORY",
    "VFS_CHARDEVICE",
    "VFS_BLOCKDEVICE",
    "VFS_SYMLINK"
};

struct vfs_node *vfs_create_node(const char *name, enum vfs_node_type type) {
    struct vfs_node *node = (struct vfs_node *)kmalloc(sizeof(struct vfs_node));
    strcpy(node->name, name);
    node->open = false;
    node->type = type;
    node->size = 0;
    node->perms = type == VFS_DIRECTORY ? 0755 : 0644;
    node->inode = 0;
    node->parent = NULL;
    node->children = NULL;
    node->next = NULL;
    node->read = NULL;
    node->write = NULL;
    node->symlink_target = NULL;
    return node;
}

void vfs_add_node(struct vfs_node *root, struct vfs_node *node) {
    if (!root) root = vfs_root;

    node->parent = root;
    if (root->children == NULL) {
        root->children = node;
    } else {
        struct vfs_node *child = root->children;
        while (child->next != NULL) {
            child = child->next;
        }
        child->next = node;
    }
}

void vfs_add_device(struct vfs_node *node) {
    vfs_add_node(vfs_dev, node);
}

struct vfs_node *vfs_create_symlink(const char *name, const char *target) {
    //printf("Creating symlink '%s' with target '%s'\n", name, target);
    struct vfs_node *node = vfs_create_node(name, VFS_SYMLINK);
    if (node && target) {
        node->symlink_target = kmalloc(strlen(target) + 1);
        strcpy(node->symlink_target, target);
        node->size = strlen(target);
    }
    return node;
}

struct vfs_node *vfs_resolve_symlink(struct vfs_node *symlink, int max_depth) {
    if (!symlink || symlink->type != VFS_SYMLINK || max_depth <= 0) {
        return symlink;
    }
    if (!symlink->symlink_target) {
        return NULL;
    }
    
    struct vfs_node *target;
    if (symlink->symlink_target[0] == '/') {
        target = vfs_open(vfs_root, symlink->symlink_target);
    } else {
        target = vfs_open(symlink->parent, symlink->symlink_target);
    }
    
    if (!target) {
        dprintf("Target %s not found!\n", symlink->symlink_target);
        return NULL;
    }
    
    if (target->type == VFS_SYMLINK) {
        return vfs_resolve_symlink(target, max_depth - 1);
    }
    return target;
}

struct vfs_node* vfs_open(struct vfs_node *current, const char *path) {
    if (!path) return NULL;
    if (!current) current = vfs_root;

    if (!strcmp(path, ".")) {
        return current;
    }
    if (!strcmp(path, "..")) {
        return current->parent;
    }

    char *copy = kmalloc(strlen(path) + 1);
    strcpy(copy, path);
    char *token = strtok(copy, "/");

    struct vfs_node *node = current;
    while (token != NULL) {
        struct vfs_node *child = node->children;
        bool found = false;

        while (child != NULL) {
            if (strcmp(child->name, token) == 0) {
                node = child;

                if (node->type == VFS_SYMLINK) {
                    node = vfs_resolve_symlink(node, MAX_NESTED_SYMLINKS);
                    if (!node) {
                        kfree(copy);
                        return NULL;
                    }
                }

                found = true;
                break;
            }
            child = child->next;
        }

        if (!found) {
            kfree(copy);
            return NULL;
        }
        token = strtok(NULL, "/");
    }

    node->open = true;
    kfree(copy);
    return node;
}

int vfs_close(struct vfs_node *node) {
    uint8_t owner_perms = (node->perms >> 6) & 0x7;
    if (!(owner_perms & 0x4)) return -EACCES;
    node->open = false;
    return 0;
}

void vfs_resolve_path(char *s, struct vfs_node *node) {
    if (!node) return;

    char path[MAX_PATH] = "";
    struct vfs_node *current = node;

    while (current != NULL) {
        sprintf(path, "%s%s%s", current == vfs_root ? "" : "/", current->name, path);
        current = current->parent;
    }

    strcpy(s, path);
}

long vfs_read(struct vfs_node *node, void *buffer, long offset, size_t len) {
    if (!node || !node->open) return -1;
    if (node->read) return node->read(node, buffer, offset, len);
    return -1;
}

long vfs_write(struct vfs_node *node, void *buffer, long offset, size_t len) {
    if (!node || !node->open) return -1;
    if (node->write) return node->write(node, buffer, offset, len);
    return -1;
}

void vfs_install(void) {
    vfs_root = (struct vfs_node *)kmalloc(sizeof(struct vfs_node));
    vfs_root->type = VFS_DIRECTORY;
    vfs_root->size = 0;
    vfs_root->perms = 0;
    vfs_root->inode = 2;
    vfs_root->parent = NULL;
    vfs_root->children = NULL;
    vfs_root->next = NULL;
    vfs_root->read = NULL;
    vfs_root->write = NULL;

    vfs_dev = vfs_create_node("dev", VFS_DIRECTORY);
    vfs_add_node(vfs_root, vfs_dev);

    zero_initialize();
    ps2_initialize();
    serial_initialize();
    console_initialize();

    printf("\033[92m * \033[97mInitialized virtual filesystem\033[0m\n");
}