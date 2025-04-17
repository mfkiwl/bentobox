#include <errno.h>
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
    uint32_t block;
    uint8_t *data;
} ext2_cache;

typedef struct {
    ext2_sb    *sb;
    ext2_bgd   *bgd_table;
    ext2_cache *block_cache;
    ext2_inode *root_ino;
    uint32_t block_size;
    uint32_t bgd_count;
    uint32_t bgd_block;
    uint32_t inode_size;
    uint32_t block_cache_idx;
} ext2_fs;

ext2_fs ext2fs;
ext2_inode *root;
struct vfs_node *hda = NULL;

int ext2_get_cache(ext2_fs* fs, uint32_t block) {
    if (fs->block_cache_idx == 0) return -1;
    for (int i = 0; i < EXT_MAX_CACHE; i++) {
        if (fs->block_cache[i].block == 0)
            break;
        if (fs->block_cache[i].block == block)
            return i;
    }
    return -1;
}

void ext2_read_block(ext2_fs* fs, uint32_t block, void* buf, uint32_t count) {
    int cache_num = ext2_get_cache(fs, block);
    if (cache_num == -1) {
        char buffer[fs->block_size];
        vfs_read(hda, buffer, block * fs->block_size, fs->block_size);
        memcpy(buf, buffer, count);

        if (fs->block_cache_idx < 0x1024) {
            fs->block_cache[fs->block_cache_idx].block = block;
            fs->block_cache[fs->block_cache_idx].data = (uint8_t *)kmalloc(fs->block_size);
            memcpy(fs->block_cache[fs->block_cache_idx].data, buf, count);
            fs->block_cache_idx++;
        }
        return;
    }
    memcpy(buf, fs->block_cache[cache_num].data, count);
}

void ext2_read_inode(ext2_fs* fs, uint32_t inode, ext2_inode* in) {
    uint32_t bg = (inode - 1) / fs->sb->inodes_per_group;
    uint32_t idx = (inode - 1) % fs->sb->inodes_per_group;
    uint32_t bg_idx = (idx * fs->inode_size) / fs->block_size;

    char buf[fs->block_size];
    ext2_read_block(fs, fs->bgd_table[bg].inode_table_block + bg_idx, buf, fs->block_size);
    // now we have a "list" of inodes, we need to index our inode
    memcpy(in, (buf + (idx % (fs->block_size / fs->inode_size)) * fs->inode_size), fs->inode_size);
}

int init() {
    dprintf("%s:%d: ext2 driver v1.0\n", __FILE__, __LINE__);

    hda = vfs_open(NULL, "/dev/hda");

    ext2_sb *sb = (ext2_sb *)kmalloc(512);
    vfs_read(hda, (void *)sb, 1024, sizeof(ext2_sb));

    if (sb->signature != 0xef53) {
        dprintf("%s:%d: not an ext2 partition\n", __FILE__, __LINE__);
        return -EINVAL;
    }
    dprintf("%s:%d: partition name: %s\n", __FILE__, __LINE__, sb->vol_name);

    ext2fs.sb = sb;
    ext2fs.block_size = 1024 << sb->log2_block;
    ext2fs.block_cache = (ext2_cache *)kmalloc(EXT_MAX_CACHE * sizeof(ext2_cache));
    ext2fs.block_cache_idx = 0;
    ext2fs.bgd_count = sb->blocks_count / sb->blocks_per_group;
    if (!ext2fs.bgd_count) {
        ext2fs.bgd_count = 1;
    }
    ext2fs.bgd_block = sb->block_num + 1;
    ext2fs.bgd_table = (ext2_bgd *)kmalloc(ext2fs.bgd_count * sizeof(ext2_bgd));
    ext2_read_block(&ext2fs, ext2fs.bgd_block, ext2fs.bgd_table, ext2fs.bgd_count * sizeof(ext2_bgd));
    ext2fs.inode_size = sb->inode_size;
    
    return 0;
}

int fini() {
    kfree(ext2fs.sb);
    kfree(ext2fs.block_cache);
    kfree(ext2fs.bgd_table);
    return 0;
}

struct Module metadata = {
    .name = "ext2 driver",
    .init = init,
    .fini = fini
};