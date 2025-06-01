#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <kernel/vfs.h>
#include <kernel/mmu.h>
#include <kernel/posix.h>
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
    "VFS_BLOCKDEVICE"
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

long sys_ioctl(struct registers *r) {
    int fd = r->rdi;
    int op = r->rsi;

    switch (op) {
        case 0x5401: /* TCGETS */
            if (fd < 3) {
                return 0;
            } else {
                return -ENOTTY;
            }
            break;
        default:
            dprintf("%s:%d: %s: function 0x%lx not implemented\n", __FILE__, __LINE__, __func__, r->rdi);
            return -EINVAL;
    }
}

long sys_lseek(struct registers *r) {
    struct fd *fd = &this->fd_table[r->rdi];
    off_t offset = r->rsi;
    int whence = r->rdx;

    if (fd->node->type == VFS_CHARDEVICE) {
        return -ESPIPE;
    }

    switch (whence) {
        case SEEK_SET:
            fd->offset = offset;
            break;
        case SEEK_CUR:
            fd->offset += offset;
            break;
        case SEEK_END:
            fd->offset = fd->node->size + offset;
            break;
    }

    return fd->offset;
}

long sys_open(struct registers *r) {
    const char *pathname = (const char *)r->rdi;
    int flags = r->rsi;
    mode_t mode = r->rdx;
    (void)mode;

    return fd_open(pathname, flags);
}

long sys_close(struct registers *r) {
    return fd_close(r->rdi);
}

long sys_access(struct registers *r) {
    if (vfs_open(NULL, (const char *)r->rdi)) {
        return F_OK;
    }
    return -1;
}

long sys_getdents64(struct registers *r) {
    struct linux_dirent64 {
        uint64_t       d_ino;
        int64_t        d_off;
        unsigned short d_reclen;
        unsigned char  d_type;
        char           d_name[];
    };

    int fd_num = r->rdi;
    struct linux_dirent64 *dirp = (struct linux_dirent64 *)r->rsi;
    unsigned int count = r->rdx;

    if (fd_num < 0 || fd_num >= (signed)(sizeof this->fd_table / sizeof(struct fd)) || !this->fd_table[fd_num].node) {
        return -EBADF;
    }

    struct fd *fd = &this->fd_table[fd_num];
    struct vfs_node *dir = fd->node;

    if (dir->type != VFS_DIRECTORY) {
        return -ENOTDIR;
    }
    if (!dirp || count == 0) {
        return -EINVAL;
    }

    struct vfs_node *child = dir->children;
    int entries_to_skip = fd->offset;
    
    while (child && entries_to_skip > 0) {
        child = child->next;
        entries_to_skip--;
    }
    
    int offset = 0;
    struct linux_dirent64 *current_entry = dirp;
    
    while (child) {
        const char *name = child->name;
        int name_len = strlen(name);
        int reclen = ALIGN_UP(sizeof(struct linux_dirent64) + name_len + 1, 8);
        
        if ((unsigned)(offset + reclen) > count)
            break;
        
        current_entry->d_ino = child->inode;
        current_entry->d_off = fd->offset + 1;
        current_entry->d_reclen = reclen;
        switch (child->type) {
            case VFS_DIRECTORY:
                current_entry->d_type = 4; // DT_DIR
                break;
            case VFS_FILE:
                current_entry->d_type = 8; // DT_REG
                break;
            case VFS_CHARDEVICE:
                current_entry->d_type = 2; // DT_CHR
                break;
            case VFS_BLOCKDEVICE:
                current_entry->d_type = 6; // DT_BLK
                break;
            default:
                current_entry->d_type = 0; // DT_UNKNOWN
                break;
        }
        strcpy(current_entry->d_name, name);
        
        current_entry = (void*)current_entry + reclen;
        offset += reclen;
        child = child->next;
        fd->offset++;
    }
    
    return offset;
}

static unsigned int perms_to_mode(enum vfs_node_type type, uint16_t perms) {
    unsigned int mode = 0;
    
    switch (type) {
        case VFS_FILE:
            mode |= S_IFREG;
            break;
        case VFS_DIRECTORY:
            mode |= S_IFDIR;
            break;
        case VFS_CHARDEVICE:
            mode |= S_IFCHR;
            break;
        case VFS_BLOCKDEVICE:
            mode |= S_IFBLK;
            break;
        default:
            mode |= S_IFREG;
            break;
    }
    
    mode |= (perms & 07777);
    return mode;
}

long sys_stat(struct registers *r) {
    const char *pathname = (const char *)r->rdi;
    struct stat *statbuf = (struct stat *)r->rsi;
    
    if (!pathname || !statbuf) {
        return -EFAULT;
    }
    
    struct vfs_node *node = vfs_open(NULL, pathname);
    if (!node) {
        return -ENOENT;
    }
    
    memset(statbuf, 0, sizeof(struct stat));
    
    statbuf->st_mode = perms_to_mode(node->type, node->perms);
    statbuf->st_nlink = 0;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    
    if (node->type == VFS_FILE) {
        statbuf->st_size = node->size;
    } else if (node->type == VFS_DIRECTORY) {
        statbuf->st_size = 4096;
    } else {
        statbuf->st_size = 0;
    }
    return 0;
}