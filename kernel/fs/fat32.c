#define FS_NAME fat32
#define STORAGE_IMPL
#include "dev_storage.h"
DECLARE_fs_DEVICE(fat32);

#include "stdlib.h"
#include "printk.h"
#include "string.h"
#include "mm.h"
#include "bitsearch.h"
#include "pool.h"
#include "fat32_decl.h"

struct fat32_fs_t {
    fs_vtbl_t *vtbl;

    storage_dev_base_t *drive;

    // Device memory mapping
    char *mm_dev;

    // Partition range
    uint64_t lba_st;
    uint64_t lba_en;

    uint32_t *fat;
    uint32_t fat_size;

    uint32_t root_cluster;
    uint32_t cluster_ofs;

    uint32_t block_size;

    uint32_t sector_size;
    uint8_t sector_shift;
    uint8_t block_shift;

    // Synthetic root directory entry
    // to allow code to refer to root as a
    // directory entry
    fat32_dir_union_t root_dirent;
};

typedef struct fat32_full_lfn_t {
    fat32_dir_union_t fragments[(256 + 12) / 13 + 1];
    uint8_t lfn_entry_count;
} fat32_full_lfn_t;

typedef struct fat32_file_handle_t {
    fat32_fs_t *fs;
    fat32_dir_entry_t *dirent;

    off_t cached_offset;
    uint64_t cached_cluster;
} fat32_file_handle_t;

static fat32_fs_t fat32_mounts[16];
static unsigned fat32_mount_count;

static pool_t fat32_handles;

static int fat32_mm_fault_handler(
        void *dev, void *addr,
        uint64_t offset, uint64_t length)
{
    FS_DEV_PTR(dev);

    uint64_t sector_offset = (offset >> self->sector_shift);
    uint64_t lba = self->lba_st + sector_offset;

    printdbg("Demand paging LBA %ld at addr %p\n", lba, (void*)addr);

    return self->drive->vtbl->read_blocks(
                self->drive, addr,
                length >> self->sector_shift, lba);
}

static void *fat32_lookup_sector(fat32_fs_t *self, uint64_t lba)
{
    return self->mm_dev + (lba << self->sector_shift);
}

static uint64_t fat32_offsetof_cluster(fat32_fs_t *self, uint64_t cluster)
{
    return (self->cluster_ofs << self->sector_shift) +
            (cluster << (self->sector_shift + self->block_shift));
}

static void *fat32_lookup_cluster(fat32_fs_t *self, uint64_t cluster)
{
    return self->mm_dev + fat32_offsetof_cluster(self, cluster);
}

static uint32_t fat32_dirent_start_cluster(fat32_dir_entry_t const *de)
{
    return ((uint32_t)de->start_hi << 16) | de->start_lo;
}

// fcb_name is the space padded 11 character name with no dot,
// the representation used in dir_entry_t's name field
static uint8_t fat32_lfn_checksum(char const *fcb_name)
{
   uint16_t i;
   uint8_t sum = 0;

   for (i = 11; i; i--)
      sum = ((sum & 1) << 7) +
              (sum >> 1) +
              (uint8_t)*fcb_name++;

   return sum;
}

static void fat32_fcbname_from_lfn(
        char *fcbname, uint16_t const *lfn, size_t lfn_len)
{
    memset(fcbname, ' ', 11);

    size_t last_dot = 0;
    for (size_t i = lfn_len; !last_dot && i > 0; --i)
        if (lfn[i-1] == '.')
            last_dot = i - 1;
    if (!last_dot)
        last_dot = lfn_len;

    size_t i;
    size_t out = 0;
    for (i = 0; i < last_dot && out < 8; ++i) {
        if ((lfn[i] >= 'A' && lfn[i] <= 'Z') ||
                (lfn[i] >= '0' && lfn[i] <= '9')) {
            fcbname[out++] = lfn[i];
        } else if (lfn[i] >= 'a' && lfn[i] <= 'z') {
            fcbname[out++] = lfn[i] + ('A' - 'a');
        } else {
            fcbname[out++] = '_';
        }
    }

    if (i < last_dot) {
        fcbname[6] = '~';
        fcbname[7] = '1';
    }

    out = 8;
    for (i = last_dot + 1; i < lfn_len && out < 11; ++i) {
        if ((lfn[i] >= 'A' && lfn[i] <= 'Z') ||
                (lfn[i] >= '0' && lfn[i] <= '9')) {
            fcbname[out++] = lfn[i];
        } else if (lfn[i] >= 'a' && lfn[i] <= 'z') {
            fcbname[out++] = lfn[i] + ('A' - 'a');
        }
    }
}

static void fat32_dirents_from_name(
        fat32_full_lfn_t *full, char const *pathname, size_t name_len)
{
    memset(full, 0, sizeof(*full));

    uint16_t lfn[256];

    char const *utf8_in = pathname;
    char const *utf8_end = pathname + name_len;
    uint16_t *utf16_out = lfn;
    int codepoint;

    do {
        codepoint = utf8_to_ucs4(utf8_in, &utf8_in);
        utf16_out += ucs4_to_utf16(utf16_out, codepoint);
    } while (utf8_in < utf8_end);
    *utf16_out = 0;

    size_t len = utf16_out - lfn;

    char fcbname[11];

    fat32_fcbname_from_lfn(fcbname, lfn, len);

    uint8_t checksum = fat32_lfn_checksum(fcbname);

    uint32_t lfn_entries = (len + 12) / 13;

    int fragment_ofs = 0;
    fat32_dir_union_t *frag = full->fragments + lfn_entries - 1;
    for (size_t ofs = 0; ofs < len || fragment_ofs != 0; ++ofs) {
        uint8_t *dest;

        if (fragment_ofs == 0) {
            frag->long_entry.attr = FAT_LONGNAME;
            frag->long_entry.checksum = checksum;
        }

        if (fragment_ofs < 5) {
            dest = frag->long_entry.name +
                    (fragment_ofs << 1);
        } else if (fragment_ofs < 11) {
            dest = frag->long_entry.name2 +
                    ((fragment_ofs - 5) << 1);
        } else {
            dest = frag->long_entry.name3 +
                    ((fragment_ofs - 11) << 1);
        }

        // After writing null terminator, fill with 0xFF
        if (ofs <= len) {
            memcpy(dest, lfn + ofs, sizeof(uint16_t));
        } else {
            dest[0] = 0xFF;
            dest[1] = 0xFF;
        }

        if (++fragment_ofs == 13) {
            fragment_ofs = 0;
            --frag;
        }
    }

    memcpy(full->fragments[lfn_entries].short_entry.name,
           fcbname, sizeof(frag->short_entry.name));

    full->lfn_entry_count = lfn_entries;

    for (size_t i = 0; i < full->lfn_entry_count; ++i) {
        full->fragments[i].long_entry.ordinal =
                i == 0
                ? full->lfn_entry_count | FAT_LAST_LFN_ORDINAL
                : full->lfn_entry_count - i;
    }
}

static size_t fat32_name_from_dirents(char *pathname,
                                      fat32_full_lfn_t const *full)
{
    if (full->lfn_entry_count == 0) {
        // Copy short name
    }

    uint16_t chunk[(sizeof(full->fragments[0].long_entry.name) +
            sizeof(full->fragments[0].long_entry.name2) +
            sizeof(full->fragments[0].long_entry.name3)) /
            sizeof(uint16_t)];

    char *out = pathname;

    fat32_dir_union_t const *frag = full->fragments +
            full->lfn_entry_count - 1;

    for (size_t i = 0; i < full->lfn_entry_count; ++i, --frag) {
        memcpy(chunk,
               frag->long_entry.name,
               sizeof(frag->long_entry.name));

        memcpy(chunk + 5,
               frag->long_entry.name2,
               sizeof(frag->long_entry.name2));

        memcpy(chunk + 5 + 6,
               frag->long_entry.name3,
               sizeof(frag->long_entry.name3));

        uint16_t const *in = chunk;
        while ((in < (chunk + countof(chunk))) &&
               *in && (*in != 0xFFFFU)) {
            int codepoint = utf16_to_ucs4(in, &in);
            out += ucs4_to_utf8(out, codepoint);
        }
    }

    return out - pathname;
}

static int fat32_is_eof(uint32_t cluster)
{
    return cluster < 2 || cluster >= 0x0FFFFFF8;
}

static fat32_dir_union_t *fat32_search_dir(
        fat32_fs_t *self, uint32_t cluster,
        char const *filename, size_t name_len)
{
    fat32_full_lfn_t lfn;
    fat32_dirents_from_name(&lfn, filename, name_len);

    uint32_t de_per_cluster = self->block_size / sizeof(fat32_dir_union_t);
    size_t match_index = 0;

    uint8_t last_checksum = 0;

    while (!fat32_is_eof(cluster)) {
        fat32_dir_union_t *de = fat32_lookup_cluster(self, self->root_cluster);

        // Iterate through all of the directory entries in this cluster
        for (size_t i = 0; i < de_per_cluster; ++i, ++de) {
            // If this is a short filename entry
            if (de->long_entry.attr != FAT_LONGNAME) {
                uint8_t checksum = fat32_lfn_checksum(de->short_entry.name);
                if (!memcmp(de->short_entry.name,
                            lfn.fragments[lfn.lfn_entry_count]
                            .short_entry.name,
                            sizeof(de->short_entry.name)) &&
                        match_index == lfn.lfn_entry_count &&
                        last_checksum == checksum) {
                    // Found
                    return de;
                }

                // Start matching from the start of the searched entry
                match_index = 0;
                continue;
            }

            if (match_index < lfn.lfn_entry_count) {
                fat32_long_dir_entry_t const *le;
                le = &lfn.fragments[match_index].long_entry;

                if (le->ordinal == de->long_entry.ordinal &&
                        !memcmp((void*)le->name,
                                (void*)de->long_entry.name,
                                   sizeof(le->name)) &&
                        !memcmp((void*)le->name2,
                                (void*)de->long_entry.name2,
                                sizeof(le->name2)) &&
                        !memcmp((void*)le->name3,
                                (void*)de->long_entry.name3,
                                sizeof(le->name3))) {
                    last_checksum = de->long_entry.checksum;
                    ++match_index;
                } else {
                    match_index = 0;
                }
            } else {
                match_index = 0;
            }
        }

        cluster = self->fat[cluster];
    }

    return 0;
}

static uint32_t fat32_walk_cluster_chain(
        fat32_fs_t *self, off_t *distance,
        uint32_t cluster, uint64_t offset)
{
    uint64_t walked = 0;

    while (walked + self->block_size <= offset) {
        cluster = self->fat[cluster];
        walked += self->block_size;
    }

    if (distance)
        *distance = walked;

    return cluster;
}

static fat32_dir_union_t *fat32_lookup_dirent(
        fat32_fs_t *self, char const *pathname)
{
    uint32_t cluster = self->root_cluster;
    fat32_dir_union_t *de;

    char const *name_st = pathname;
    char const *path_end = pathname + strlen(pathname);

    if (name_st == path_end)
        return &self->root_dirent;

    char const *name_en;
    for ( ; name_st < path_end; name_st = name_en + 1) {
        name_en = memchr(name_st, '/', path_end - name_st);

        if (!name_en)
            name_en = path_end;

        size_t name_len = name_en - name_st;

        if (name_len == 0)
            break;

        de = fat32_search_dir(self, cluster, name_st, name_len);

        if (!de)
            return 0;

        cluster = fat32_dirent_start_cluster(&de->short_entry);
    }

    return de;
}

static fat32_file_handle_t *fat32_create_handle(
        fat32_fs_t *self, char const *path)
{
    fat32_dir_union_t *de = fat32_lookup_dirent(self, path);
    (void)de;

    if (unlikely(!de))
        return 0;

    fat32_file_handle_t *file = pool_alloc(&fat32_handles);

    file->fs = self;
    file->dirent = &de->short_entry;

    file->cached_offset = 0;
    file->cached_cluster = fat32_dirent_start_cluster(&de->short_entry);

    return file;
}

static ssize_t fat32_internal_read(
        fat32_fs_t *self, fat32_file_handle_t *file,
        void *buf, size_t size, off_t offset)
{
    char *out = buf;
    ssize_t result = 0;

    off_t cached_end = file->cached_offset + self->block_size;
    while (size > 0) {
        if ((offset >= cached_end) &&
                (offset < cached_end + self->block_size)) {
            // Move to next cluster
            file->cached_offset += self->block_size;
            file->cached_cluster = self->fat[file->cached_cluster];
            cached_end += self->block_size;
        } else if ((offset < file->cached_offset) ||
                   (offset >= cached_end)) {
            // Offset cache miss
            file->cached_cluster = fat32_walk_cluster_chain(
                        self, &file->cached_offset,
                        fat32_dirent_start_cluster(file->dirent),
                        offset);
            cached_end = file->cached_offset + self->block_size;
        }

        size_t avail;

        if (offset < cached_end)
            avail = cached_end - offset;
        else
            avail = 0;

        assert(avail <= self->block_size);

        if (avail > size)
            avail = size;

        size_t cluster_data_ofs = fat32_offsetof_cluster(
                    self, file->cached_cluster);

        memcpy(out, self->mm_dev + cluster_data_ofs +
               (offset - file->cached_offset), avail);

        offset += avail;
        size -= avail;
        out += avail;
        result += avail;
    }

    return result;
}

//
// Startup and shutdown

static void *fat32_mount(fs_init_info_t *conn)
{
    if (fat32_mount_count == 0)
        pool_create(&fat32_handles, sizeof(fat32_file_handle_t), 512);

    if (unlikely(fat32_mount_count == countof(fat32_mounts))) {
        printk("Too many FAT32 mounts\n");
        return 0;
    }

    fat32_fs_t *self = fat32_mounts + fat32_mount_count++;

    self->vtbl = &fat32_fs_device_vtbl;

    self->drive = conn->drive;

    self->sector_size = self->drive->vtbl->info(
                self->drive, STORAGE_INFO_BLOCKSIZE);

    autofree void *sector_buffer = malloc(self->sector_size);
    fat32_bpb_data_t bpb;

    self->lba_st = conn->part_st;
    self->lba_en = conn->part_st + conn->part_len;

    self->drive->vtbl->read_blocks(
                self->drive, sector_buffer, 1, self->lba_st);

    // Pass 0 partition cluster to get partition relative values
    fat32_parse_bpb(&bpb, 0, sector_buffer);

    self->root_cluster = bpb.root_dir_start;

    memset(&self->root_dirent, 0, sizeof(self->root_dirent));
    self->root_dirent.short_entry.start_lo =
            (self->root_cluster) & 0xFFFF;
    self->root_dirent.short_entry.start_hi =
            (self->root_cluster >> 16) & 0xFFFF;

    // Sector offset of cluster 0
    self->cluster_ofs = bpb.cluster_begin_lba;

    self->sector_shift = bit_log2_n_32(self->sector_size);
    self->block_shift = bit_log2_n_32(bpb.sec_per_cluster);
    self->block_size = self->sector_size << self->block_shift;

    self->mm_dev = mmap_register_device(
                self, self->block_size, conn->part_len,
                PROT_READ, fat32_mm_fault_handler);

    self->fat_size = bpb.sec_per_fat << self->sector_shift;
    self->fat = fat32_lookup_sector(self, bpb.first_fat_lba);

    return self;
}

static void fat32_unmount(fs_base_t *dev)
{
    FS_DEV_PTR(dev);
    munmap(self->mm_dev,
           (uint64_t)(self->lba_en - self->lba_st)
           << self->sector_shift);
}

//
// Scan directories

static int fat32_opendir(fs_base_t *dev,
                         fs_file_info_t **fi,
                         fs_cpath_t path)
{
    FS_DEV_PTR(dev);

    fat32_file_handle_t *file = fat32_create_handle(self, path);

    if (!file)
        return -1;

    *fi = file;

    return 0;
}

static ssize_t fat32_readdir(fs_base_t *dev,
                             fs_file_info_t *fi,
                             dirent_t *buf,
                             off_t offset)
{
    FS_DEV_PTR(dev);

    fat32_full_lfn_t lfn;
    size_t index;
    size_t distance;

    memset(&lfn, 0, sizeof(lfn));

    for (index = 0, distance = 0;
         sizeof(lfn.fragments[index]) == fat32_internal_read(
             self, fi, lfn.fragments + index, sizeof(lfn.fragments[index]),
             offset + sizeof(lfn.fragments[index]) * index);
         ++distance, ++index) {
        if (lfn.fragments[index].short_entry.name[0] == 0)
            break;

        if (unlikely(index + 1 >= countof(lfn.fragments))) {
            // Invalid
            index = 0;
            continue;
        }

        if (lfn.fragments[index].short_entry.attr == FAT_LONGNAME)
            continue;

        if (lfn.fragments[index].short_entry.name[0] == FAT_DELETED_FLAG) {
            index = 0;
            continue;
        }

        lfn.lfn_entry_count = index;
        ++distance;
        break;
    }

    fat32_name_from_dirents(buf->d_name, &lfn);

    return distance * sizeof(fat32_dir_entry_t);
}

static int fat32_releasedir(fs_base_t *dev,
                            fs_file_info_t *fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
    return -1;
}

//
// Read directory entry information

static int fat32_getattr(fs_base_t *dev,
                         fs_cpath_t path, fs_stat_t* stbuf)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)stbuf;
    return -1;
}

static int fat32_access(fs_base_t *dev,
                        fs_cpath_t path, int mask)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mask;
    return -1;
}

static int fat32_readlink(fs_base_t *dev,
                          fs_cpath_t path, char* buf, size_t size)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)buf;
    (void)size;
    return -1;
}

//
// Modify directories

static int fat32_mknod(fs_base_t *dev,
                       fs_cpath_t path,
                       fs_mode_t mode, fs_dev_t rdev)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mode;
    (void)rdev;
    return -1;
}

static int fat32_mkdir(fs_base_t *dev,
                       fs_cpath_t path, fs_mode_t mode)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mode;
    return -1;
}

static int fat32_rmdir(fs_base_t *dev,
                       fs_cpath_t path)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    return -1;
}

static int fat32_symlink(fs_base_t *dev,
                         fs_cpath_t to, fs_cpath_t from)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)to;
    (void)from;
    return -1;
}

static int fat32_rename(fs_base_t *dev,
                        fs_cpath_t from, fs_cpath_t to)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)from;
    (void)to;
    return -1;
}

static int fat32_link(fs_base_t *dev,
                      fs_cpath_t from, fs_cpath_t to)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)from;
    (void)to;
    return -1;
}

static int fat32_unlink(fs_base_t *dev,
                        fs_cpath_t path)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    return -1;
}

//
// Modify directory entries

static int fat32_chmod(fs_base_t *dev,
                       fs_cpath_t path, fs_mode_t mode)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mode;
    return -1;
}

static int fat32_chown(fs_base_t *dev,
                       fs_cpath_t path, fs_uid_t uid, fs_gid_t gid)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)uid;
    (void)gid;
    return -1;
}

static int fat32_truncate(fs_base_t *dev,
                          fs_cpath_t path, off_t size)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)size;
    return -1;
}

static int fat32_utimens(fs_base_t *dev,
                         fs_cpath_t path,
                         const fs_timespec_t *ts)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)ts;
    return -1;
}

//
// Open/close files

static int fat32_open(fs_base_t *dev,
                      fs_file_info_t **fi,
                      fs_cpath_t path)
{
    FS_DEV_PTR(dev);

    fat32_file_handle_t *file = fat32_create_handle(self, path);

    if (!file)
        return -1;

    *fi = file;

    return 0;
}

static int fat32_release(fs_base_t *dev,
                         fs_file_info_t *fi)
{
    FS_DEV_PTR_UNUSED(dev);

    pool_free(&fat32_handles, fi);
    return 0;
}

//
// Read/write files

static ssize_t fat32_read(fs_base_t *dev,
                          fs_file_info_t *fi,
                          char *buf,
                          size_t size,
                          off_t offset)
{
    FS_DEV_PTR(dev);
    return fat32_internal_read(self, fi, buf, size, offset);
}

static ssize_t fat32_write(fs_base_t *dev,
                           fs_file_info_t *fi,
                           char const *buf,
                           size_t size,
                           off_t offset)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    return -1;
}

static int fat32_ftruncate(fs_base_t *dev,
                           fs_file_info_t *fi,
                           off_t offset)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)offset;
    (void)fi;
    return -1;
}

//
// Query open file

static int fat32_fstat(fs_base_t *dev,
                       fs_file_info_t *fi,
                       fs_stat_t *st)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
    (void)st;
    return -1;
}

//
// Sync files and directories and flush buffers

static int fat32_fsync(fs_base_t *dev,
                       fs_file_info_t *fi,
                       int isdatasync)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)isdatasync;
    (void)fi;
    return 0;
}

static int fat32_fsyncdir(fs_base_t *dev,
                          fs_file_info_t *fi,
                          int isdatasync)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)isdatasync;
    (void)fi;
    return 0;
}

static int fat32_flush(fs_base_t *dev,
                       fs_file_info_t *fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
    return 0;
}

//
// lock/unlock file

static int fat32_lock(fs_base_t *dev,
                      fs_file_info_t *fi,
                      int cmd,
                      fs_flock_t* locks)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
    (void)cmd;
    (void)locks;
    return -1;
}

//
// Get block map

static int fat32_bmap(fs_base_t *dev,
                      fs_cpath_t path,
                      size_t blocksize,
                      uint64_t* blockno)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)blocksize;
    (void)blockno;
    return -1;
}

//
// Get filesystem information

static int fat32_statfs(fs_base_t *dev,
                        fs_statvfs_t* stbuf)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)stbuf;
    return -1;
}

//
// Read/Write/Enumerate extended attributes

static int fat32_setxattr(fs_base_t *dev,
                          fs_cpath_t path,
                          char const* name,
                          char const* value,
                          size_t size, int flags)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    return -1;
}

static int fat32_getxattr(fs_base_t *dev,
                          fs_cpath_t path,
                          char const* name,
                          char* value,
                          size_t size)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    return -1;
}

static int fat32_listxattr(fs_base_t *dev,
                           fs_cpath_t path,
                           char const* list,
                           size_t size)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)list;
    (void)size;
    return -1;
}

//
// ioctl API

static int fat32_ioctl(fs_base_t *dev,
                       fs_file_info_t *fi,
                       int cmd,
                       void* arg,
                       unsigned int flags,
                       void* data)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)cmd;
    (void)arg;
    (void)fi;
    (void)flags;
    (void)data;
    return -1;
}

//
//

static int fat32_poll(fs_base_t *dev,
                      fs_file_info_t *fi,
                      fs_pollhandle_t* ph,
                      unsigned* reventsp)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
    (void)ph;
    (void)reventsp;
    return -1;
}

REGISTER_fs_DEVICE(fat32);
