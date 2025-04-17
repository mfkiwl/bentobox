#include <errno.h>
#include <kernel/mmu.h>
#include <kernel/vfs.h>
#include <kernel/malloc.h>
#include <kernel/module.h>
#include <kernel/printf.h>
#include <kernel/string.h>

typedef struct ext2_sb {
    uint32_t inodes;
    uint32_t blocks;
    uint32_t reserved_blocks;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t block_num;
    uint32_t log2_block;
    uint32_t log2_fragment;
    uint32_t blocks_per_group;
    uint32_t fragments_per_group;
    uint32_t inodes_per_group;
    uint32_t last_mount_time;
    uint32_t last_write_time;
    uint16_t mount_times_check;
    uint16_t mount_times_allowed;
    uint16_t signature;
    uint16_t state;
    uint16_t err_handler;
    uint16_t minor_ver;
    uint32_t last_consistency_check;
    uint32_t consistency_interval;
    uint32_t os_id;
    uint32_t major_ver;
    uint16_t reserved_blocks_uid;
    uint16_t reserved_blocks_gid;

    uint32_t first_inode;
    uint16_t inode_size;
    uint16_t sb_block_group;
    uint32_t optional_features;
    uint32_t required_features;
    uint32_t features_for_rw;
    uint8_t  fs_id[16];
    uint8_t  vol_name[16];
    uint8_t  last_mount_point[64];
    uint32_t compression_algorithms_used;
    uint8_t  preallocated_blocks_for_files;
    uint8_t  preallocated_blocks_for_dirs;
    uint16_t unused;
    uint8_t  journal_id[16];
    uint32_t journal_inode;
    uint32_t journal_device;
    uint32_t head_of_oprphan_inode_list;
} ext2_sb;

typedef struct ext2_bgd {
    uint32_t bitmap_block;
    uint32_t bitmap_inode;
    uint32_t inode_table;
    uint16_t free_blocks;
    uint16_t free_inodes;
    uint16_t directories;
    uint8_t  unused[14];
} ext2_bgd;

typedef struct ext2_inode {
    uint16_t type_permissions;
    uint16_t user_id;
    uint32_t size;
    uint32_t last_access_time;
    uint32_t creation_time;
    uint32_t last_modify_time;
    uint32_t deletion_time;
    uint16_t group_id;
    uint16_t hard_links;
    uint32_t sectors;
    uint32_t flags;
    uint32_t os_specific;
    uint32_t direct_block_ptr[12];
    uint32_t singly_block_ptr;
    uint32_t doubly_block_ptr;
    uint32_t triply_block_ptr;
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t fragment_addr;
    uint32_t os_specific_2[3];
} ext2_inode;

typedef struct ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[];
} ext2_dir_entry;

typedef struct ext2_fs {
    struct ext2_sb *sb;
    struct ext2_bgd *bgd_table;
    struct ext2_inode *root_inode;
    uint32_t block_size;
    uint32_t bgd_count;
    uint32_t bgd_block;
    uint32_t inode_size;
} ext2_fs;

ext2_fs ext2fs;
ext2_inode *root;
struct vfs_node *hda = NULL;

int read_block(struct ext2_fs *fs, size_t block, void *buffer, size_t count) {
    return vfs_read(hda, buffer, block * fs->block_size, count);
}

void read_inode(ext2_fs* fs, size_t inode_no, ext2_inode *inode) {
    size_t bg = (inode_no - 1) / fs->sb->inodes_per_group;
    size_t idx = (inode_no - 1) % fs->sb->inodes_per_group;
    size_t bg_idx = (idx * fs->inode_size) / fs->block_size;

    uint8_t buffer[fs->block_size];
    read_block(fs, fs->bgd_table[bg].inode_table + bg_idx, buffer, fs->block_size);
    memcpy(inode, (buffer + (idx % (fs->block_size / fs->inode_size)) * fs->inode_size), fs->inode_size);
}

void read_singly_blocks(ext2_fs* fs, uint32_t singly_block_id, uint8_t* buf) {
    uint32_t* blocks = (uint32_t*)kmalloc(fs->block_size);
    uint32_t block_count = fs->block_size / 4; // on 1KB Blocks, 13 - 268 (or 256 blocks)
    read_block(fs, singly_block_id, blocks, fs->block_size);
    for (int i = 0; i < block_count; i++) {
        if (blocks[i] == 0) break;
        read_block(fs, blocks[i], buf + (i * fs->block_size), fs->block_size);
    }
    kfree(blocks);
}

void read_inode_blocks(ext2_fs* fs, ext2_inode* in, uint8_t* buf) {
    // TODO: Read singly, doubly and triply linked blocks
    for (int i = 0; i < 12; i++) {
        uint32_t block = in->direct_block_ptr[i];
        if (block == 0) break;
        read_block(fs, block, buf + (i * fs->block_size), fs->block_size);
    }
    if (in->singly_block_ptr != 0) {
        read_singly_blocks(fs, in->singly_block_ptr, buf + (12 * fs->block_size));
    }
}

int init() {
    dprintf("%s:%d: ext2 driver v1.0\n", __FILE__, __LINE__);

    hda = vfs_open(NULL, "/dev/hda");

    ext2_sb *sb = (ext2_sb *)kmalloc(512);
    vfs_read(hda, (void *)sb, 1024, 512);

    if (sb->signature != 0xef53) {
        dprintf("%s:%d: not an ext2 partition\n", __FILE__, __LINE__);
        return -EINVAL;
    }
    dprintf("%s:%d: partition name: %s\n", __FILE__, __LINE__, sb->vol_name);

    ext2fs.sb = sb;
    ext2fs.block_size = (1024 << sb->log2_block);
    ext2fs.bgd_count = sb->blocks / sb->blocks_per_group;
    ext2fs.bgd_block = sb->block_num + 1;
    ext2fs.bgd_table = (struct ext2_bgd *)kmalloc(sizeof(struct ext2_bgd) * ext2fs.bgd_count);
    ext2fs.inode_size = sb->major_ver == 1 ? sb->inode_size : 128;
    
    root = (ext2_inode *)kmalloc(ext2fs.inode_size);
    read_inode(&ext2fs, 2, root);

    return 0;
}

struct Module metadata = {
    .name = "ext2 driver",
    .init = init
};