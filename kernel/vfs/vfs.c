#include <stddef.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <kernel/string.h>
#include <kernel/printf.h>

extern void zero_initialize(void);
extern void serial_tty_install(void);

struct vfs_node *vfs_root;
struct vfs_node *vfs_dev;

const char *vfs_types[] = {
    "NONE",
    "VFS_FILE",
    "VFS_DIRECTORY",
    "VFS_CHARDEVICE",
    "VFS_BLOCKDEVICE"
};

struct vfs_node *vfs_create_node(const char *name, enum vfs_node_type type) {
    struct vfs_node *node = (struct vfs_node *)kmalloc(sizeof(struct vfs_node));
    strcpy(node->name, name);
    node->type = type;
    node->size = 0;
    node->perms = 0;
    node->parent = NULL;
    node->children = NULL;
    node->next = NULL;
    node->read = NULL;
    node->write = NULL;

    dprintf("%s:%d: created node '%s' with type %s\n", __FILE__, __LINE__, name, vfs_types[type]);
    return node;
}

void vfs_add_node(struct vfs_node *root, struct vfs_node *node) {
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

struct vfs_node* vfs_open(struct vfs_node *current, const char *path) {
    if (!current || !path) return NULL;

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
                found = true;
                break;
            }
            child = child->next;
        }

        if (!found) return NULL;
        token = strtok(NULL, "/");
    }

    node->open = true;
    kfree(copy);
    return node;
}

char *vfs_get_path(struct vfs_node *node) {
    if (!node) return NULL;

    char path[MAX_PATH] = "";
    struct vfs_node *current = node;

    while (current != NULL) {
        sprintf(path, "%s%s%s", current == vfs_root ? "" : "/", current->name, path);
        current = current->parent;
    }

    char *result = kmalloc(strlen(path) + 1);
    strcpy(result, path);
    return result;
}

int32_t vfs_read(struct vfs_node *node, void *buffer, uint32_t len) {
    if (!node) return -1;
    if (node->read) return node->read(node, buffer, len);
    return -1;
}

int32_t vfs_write(struct vfs_node *node, void *buffer, uint32_t len) {
    if (!node) return -1;
    if (node->write) return node->write(node, buffer, len);
    return -1;
}

void vfs_install(void) {
    vfs_root = (struct vfs_node *)kmalloc(sizeof(struct vfs_node));
    vfs_root->type = VFS_DIRECTORY;
    vfs_root->size = 0;
    vfs_root->perms = 0;
    vfs_root->parent = NULL;
    vfs_root->children = NULL;
    vfs_root->next = NULL;
    vfs_root->read = NULL;
    vfs_root->write = NULL;

    vfs_dev = vfs_create_node("dev", VFS_DIRECTORY);
    vfs_add_node(vfs_root, vfs_dev);

    zero_initialize();
    serial_tty_install();

    printf("\033[92m * \033[97mInitialized virtual filesystem\033[0m\n");
}