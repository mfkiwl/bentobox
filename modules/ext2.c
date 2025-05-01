#include <errno.h>
#include <stdint.h>
#include <kernel/mmu.h>
#include <kernel/vfs.h>
#include <kernel/malloc.h>
#include <kernel/module.h>
#include <kernel/printf.h>
#include <kernel/string.h>

#define EXT_FIFO        0x1000
#define EXT_CHAR_DEV    0x2000
#define EXT_DIRECTORY   0x4000
#define EXT_BLOCK_DEV   0x6000
#define EXT_FILE        0x8000
#define EXT_SYM_LINK    0xA000
#define EXT_UNIX_SOCKET 0xC000

#define EXT_MAX_CACHE   0x1024

typedef struct {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t su_resv_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t block_num;
    uint32_t log2_block;
    uint32_t log2_frag;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t last_mount_time;
    uint32_t last_write_time;
    uint16_t mount_times_check;
    uint16_t mount_times_allowed;
    uint16_t signature;
    uint16_t state;
    uint16_t err_handle;
    uint16_t minor_ver;
    uint32_t last_consistency_check;
    uint32_t consistency_interval;
    uint32_t os_id;
    uint32_t major_ver;
    uint16_t resv_blocks_user_id;
    uint16_t resv_blocks_group_id;

    uint32_t first_inode;
    uint16_t inode_size;
    uint16_t sb_bgd;
    uint32_t opt_features;
    uint32_t req_features;
    uint32_t mount_features;
    uint8_t  fs_id[16];
    char     vol_name[16];
    char     vol_path_mount[64];
    uint32_t compression_algo;
    uint8_t  preallocate_blocks_file;
    uint8_t  preallocate_blocks_dir;
    uint16_t unused;
    uint64_t journal_id[2];
    uint32_t journal_inode;
    uint32_t journal_device;
    uint32_t orphan_inode_list;
    uint8_t  unused_ext[787];
} ext2_sb;

typedef struct {
    uint16_t type_perms;
    uint16_t user_id;
    uint32_t size;
    uint32_t last_access_time;
    uint32_t creation_time;
    uint32_t mod_time;
    uint32_t deletion_time;
    uint16_t group_id;
    uint16_t hard_link_count;
    uint32_t sector_count;
    uint32_t flags;
    uint32_t os_spec;
    uint32_t direct_block_ptr[12];
    uint32_t singly_block_ptr;
    uint32_t doubly_block_ptr;
    uint32_t triply_block_ptr;
    uint32_t gen_number;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t frag_block_addr;
    uint8_t  os_spec2[12];
} ext2_inode;

typedef struct {
    uint32_t inode;
    uint16_t total_size;
    uint8_t  name_len;
    uint8_t  type;
    uint8_t  name[];
} ext2_dirent;

typedef struct {
    uint32_t bitmap_block;
    uint32_t bitmap_inode;
    uint32_t inode_table_block;
    uint16_t free_blocks;
    uint16_t free_inodes;
    uint16_t directories_count;
    uint16_t pad;
    uint8_t  resv[12];
} ext2_bgd;

typedef struct {
    ext2_sb    *sb;
    ext2_bgd   *bgd_table;
    ext2_inode *root_inode;
    uint32_t block_size;
    uint32_t bgd_count;
    uint32_t bgd_block;
    uint32_t inode_size;
} ext2_fs;

ext2_fs ext2fs;
struct vfs_node *hda = NULL;

void ext2_read_block(ext2_fs *fs, uint32_t block, void* buf, uint32_t count) {
    char buffer[fs->block_size];
    vfs_read(hda, buffer, block * fs->block_size, fs->block_size);
    memcpy(buf, buffer, count);
}

void ext2_read_inode(ext2_fs *fs, uint32_t inode, ext2_inode *in) {
    uint32_t bg = (inode - 1) / fs->sb->inodes_per_group;
    uint32_t idx = (inode - 1) % fs->sb->inodes_per_group;
    uint32_t bg_idx = (idx * fs->inode_size) / fs->block_size;

    char buf[fs->block_size];
    ext2_read_block(fs, fs->bgd_table[bg].inode_table_block + bg_idx, buf, fs->block_size);
    memcpy(in, (buf + (idx % (fs->block_size / fs->inode_size)) * fs->inode_size), fs->inode_size);
}

uint32_t ext2_read_singly_blocks(ext2_fs *fs, uint32_t block, uint8_t *buf, uint32_t count) {
    uint32_t *blocks = (uint32_t *)kmalloc(fs->block_size);
    uint32_t block_count = fs->block_size / 4;
    uint32_t count_block = DIV_CEILING(count, fs->block_size);
    ext2_read_block(fs, block, blocks, fs->block_size);
    uint32_t remaining = count;
    for (uint32_t i = 0; i < count_block; i++) {
        if (i == block_count) break;
        if (blocks[i] == 0) break;
        ext2_read_block(fs, blocks[i], buf + (i * fs->block_size), (remaining > fs->block_size ? fs->block_size : remaining));
        remaining -= fs->block_size;
    }
    kfree(blocks);
    return remaining;
}

void ext2_read_inode_blocks(ext2_fs *fs, ext2_inode *in, uint8_t *buf, uint32_t count) {
    uint32_t remaining = count;
    uint32_t blocks = DIV_CEILING(count, fs->block_size);
    for (uint32_t i = 0; i < (blocks > 12 ? 12 : blocks); i++) {
        uint32_t block = in->direct_block_ptr[i];
        if (block == 0) break;
        ext2_read_block(fs, block, buf + (i * fs->block_size), (remaining > fs->block_size ? fs->block_size : remaining));
        remaining -= fs->block_size;
    }
    if (blocks > 12) {
        if (in->singly_block_ptr != 0) {
            ext2_read_singly_blocks(fs, in->singly_block_ptr, buf + (12 * fs->block_size), remaining);
        }
    }
}

int32_t ext2_read(struct vfs_node *node, void *buffer, uint32_t offset, uint32_t len) {
    ext2_inode *inode = (ext2_inode *)kmalloc(ext2fs.inode_size);
    ext2_read_inode(&ext2fs, node->inode, inode);
    if (!inode) {
        return -ENOENT;
    }

    if (offset + len > inode->size) {
        len = inode->size - offset;
    }

    uint8_t *buf = (uint8_t *)kmalloc(inode->size);
    ext2_read_inode_blocks(&ext2fs, inode, buf, offset + len);
    memcpy(buffer, buf + offset, len);

    kfree(buf);
    kfree(inode);
    return len;
}

void ext2_mount(ext2_fs *fs, struct vfs_node *parent, uint32_t inode_num) {
    ext2_inode *inode = (ext2_inode *)kmalloc(fs->inode_size);
    ext2_read_inode(fs, inode_num, inode);
    
    uint32_t block_size = fs->block_size;

    for (int i = 0; i < 12; i++) {
        if (inode->direct_block_ptr[i] == 0)
            continue;

        uint8_t block[block_size];
        ext2_read_block(fs, inode->direct_block_ptr[i], block, block_size);

        uint32_t offset = 0;
        while (offset < block_size) {
            ext2_dirent *entry = (ext2_dirent *)(block + offset);
            if (entry->inode == 0)
                break;

            char name[256];
            memcpy(name, entry->name, entry->name_len);
            name[entry->name_len] = '\0';

            ext2_inode *child = (ext2_inode *)kmalloc(fs->inode_size);
            ext2_read_inode(fs, entry->inode, child);

            uint16_t type = child->type_perms & 0xF000;
            uint32_t vfs_type = NONE;

            switch (type) {
                case EXT_FILE:
                    vfs_type = VFS_FILE;
                    break;
                case EXT_DIRECTORY:
                    vfs_type = VFS_DIRECTORY;
                    break;
                case EXT_CHAR_DEV:
                    vfs_type = VFS_CHARDEVICE;
                    break;
                case EXT_BLOCK_DEV:
                    vfs_type = VFS_BLOCKDEVICE;
                    break;
            }

            struct vfs_node *node = vfs_create_node(name, vfs_type);
            node->size = child->size;
            node->inode = entry->inode;
            node->read = ext2_read;
            vfs_add_node(parent, node);

            if (type == EXT_DIRECTORY && strcmp(name, "lost+found") != 0 && !(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)) {
                ext2_mount(fs, node, entry->inode);
            }

            offset += entry->total_size;
            kfree(child);
        }
    }
    
    kfree(inode);
}

int init() {
    extern char load_addr[];
    dprintf("%s:%d: ext2 driver v1.0 @ 0x%lx\n", __FILE__, __LINE__, &load_addr);

    hda = vfs_open(NULL, "/dev/hda");

    ext2_sb *sb = (ext2_sb *)kmalloc(512);
    vfs_read(hda, (void *)sb, 1024, sizeof(ext2_sb));

    if (sb->signature != 0xef53) {
        dprintf("%s:%d: not an ext2 partition\n", __FILE__, __LINE__);
        return -EINVAL;
    }

    ext2fs.sb = sb;
    ext2fs.block_size = 1024 << sb->log2_block;
    ext2fs.bgd_count = sb->blocks_count / sb->blocks_per_group;
    if (!ext2fs.bgd_count) {
        ext2fs.bgd_count = 1;
    }
    ext2fs.bgd_block = sb->block_num + 1;
    ext2fs.bgd_table = (ext2_bgd *)kmalloc(ext2fs.bgd_count * sizeof(ext2_bgd));
    ext2_read_block(&ext2fs, ext2fs.bgd_block, ext2fs.bgd_table, ext2fs.bgd_count * sizeof(ext2_bgd));
    ext2fs.inode_size = sb->inode_size;

    ext2fs.root_inode = (ext2_inode *)kmalloc(ext2fs.inode_size);
    ext2_read_inode(&ext2fs, 2, ext2fs.root_inode);

    ext2_mount(&ext2fs, vfs_root, 2);
    return 0;
}

int fini() {
    dprintf("%s:%d: Goodbye!\n", __FILE__, __LINE__);
    kfree(ext2fs.sb);
    kfree(ext2fs.bgd_table);
    return 0;
}

struct Module metadata = {
    .name = "ext2 driver",
    .init = init,
    .fini = fini
};