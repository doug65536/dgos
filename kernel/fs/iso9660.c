#define FS_NAME iso9660
#define STORAGE_IMPL
#include "dev_storage.h"
DECLARE_fs_DEVICE(iso9660);

#include "iso9660_decl.h"
#include "threadsync.h"
#include "bitsearch.h"
#include "mm.h"
#include "bswap.h"
#include "string.h"
#include "pool.h"
#include "bsearch.h"
#include "printk.h"

struct iso9660_fs_t {
    fs_vtbl_t *vtbl;

    storage_dev_base_t *drive;

    // Partition range
    uint64_t lba_st;
    uint64_t lba_len;

    // Root
    uint32_t root_lba;
    uint32_t root_bytes;

    // Path table
    uint32_t pt_lba;
    uint32_t pt_bytes;

    int (*name_convert)(void *encoded_buf,
                        char const *utf8);
    int (*lookup_path_cmp)(void const *v,
                           void const *k,
                           void *s);
    int (*name_compare)(void const *name, size_t name_len,
                        char const *find, size_t find_len);
    void (*name_copy)(char *out, void *in, size_t len);

    // Path table and path table lookup table
    iso9660_pt_rec_t *pt;
    iso9660_pt_rec_t **pt_ptrs;

    uint32_t pt_alloc_size;
    uint32_t pt_count;

    // From the drive
    uint32_t sector_size;

    // >= 2KB
    uint32_t block_size;

    // Device memory mapping
    char *mm_dev;

    // log2(sector_size)
    uint8_t sector_shift;

    // log2(num_sectors_per_block)
    uint8_t block_shift;
};

static iso9660_fs_t iso9660_mounts[16];
static unsigned iso9660_mount_count;

static pool_t iso9660_handles;

typedef struct iso9660_dir_handle_t {
    iso9660_fs_t *fs;
    iso9660_dir_ent_t *dirent;
    char *content;
} iso9660_dir_handle_t;

typedef struct iso9660_file_handle_t {
    iso9660_fs_t *fs;
    iso9660_dir_ent_t *dirent;
    char *content;
} iso9660_file_handle_t;

typedef union iso9660_handle_t {
    iso9660_fs_t *fs;
    iso9660_file_handle_t file;
    iso9660_dir_handle_t dir;
} iso9660_handle_t;

typedef struct iso9660_path_key_t {
    char const *name;
    size_t len;
    size_t parent;
} iso9660_path_key_t;

static uint64_t iso9660_dirent_size(iso9660_dir_ent_t const *de)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (de->size_hi_le << 16) |
            de->size_lo_le;
#else
    return (de->size_hi_be << 16) |
            de->size_lo_be;
#endif
}

static uint64_t iso9660_dirent_lba(iso9660_dir_ent_t const *de)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (de->lba_hi_le << 16) |
            de->lba_lo_le;
#else
    return (de->lba_hi_be << 16) |
            de->lba_lo_be;
#endif
}

static uint64_t iso9660_pt_rec_lba(iso9660_pt_rec_t const *pt_rec)
{
    return (pt_rec->lba_hi << 16) |
            pt_rec->lba_lo;
}

static int iso9660_name_to_ascii(
        void *ascii_buf,
        char const *utf8)
{
    char *ascii = ascii_buf;
    int codepoint;
    int out = 0;
    char *lastdot = 0;

    for (int i = 0; *utf8 && i < ISO9660_MAX_NAME; ++i) {
        codepoint = utf8_to_ucs4(utf8, &utf8);

        if (codepoint == '.') {
            if (lastdot)
                *lastdot = '_';
            lastdot = ascii + out;
        }

        if (codepoint >= 'a' && codepoint <= 'z')
            codepoint += 'A' - 'a';

        if ((codepoint >= '0' && codepoint <= '9') ||
                (codepoint >= 'A' && codepoint <= 'Z') ||
                (codepoint == '.'))
            ascii[out++] = (char)codepoint;
        else
            ascii[out++] = '_';
    }

    return out;
}

static int iso9660_name_to_utf16be(
        void *utf16be_buf,
        char const *utf8)
{
    uint16_t *utf16be = utf16be_buf;
    int codepoint;
    uint16_t utf16buf[3];
    int utf16sz;
    int out = 0;

    for (int i = 0; *utf8 && i < ISO9660_MAX_NAME; ++i) {
        codepoint = utf8_to_ucs4(utf8, &utf8);

        utf16sz = ucs4_to_utf16(utf16buf, codepoint);

        if (utf16sz > 0)
            utf16be[out++] = htons(utf16buf[0]);
        if (utf16sz > 1)
            utf16be[out++] = htons(utf16buf[1]);
    }

    return out;
}

static void iso9660_name_copy_ascii(
        char *out, void *in, size_t len)
{
    memcpy(out, in, len);
    out[len] = 0;
}

static void iso9660_name_copy_utf16be(
        char *out, void *in, size_t len)
{
    uint16_t const *name = in;
    size_t name_len = len >> 1;
    uint16_t const *name_end = name + name_len;

    while (name < name_end) {
        int codepoint = utf16be_to_ucs4(name, &name);
        out += ucs4_to_utf8(out, codepoint);
    }
}

static uint32_t iso9660_round_up(
        uint32_t n,
        uint8_t log2_size)
{
    uint32_t size = 1U << log2_size;
    uint32_t mask = size - 1;
    return (n + mask) & ~mask;
}

static uint32_t iso9660_walk_pt(
        iso9660_fs_t *self,
        void (*cb)(uint32_t i,
                   iso9660_pt_rec_t *rec,
                   void *p),
        void *p)
{
    uint32_t i = 0;
    for (iso9660_pt_rec_t *pt_rec = self->pt,
         *pt_end = (void*)((char*)self->pt + self->pt_bytes);
         pt_rec < pt_end;
         ++i,
         pt_rec = (void*)((char*)pt_rec +
                          ((offsetof(iso9660_pt_rec_t, name) +
                          pt_rec->di_len + 1) & -2))) {
        if (cb)
            cb(i, pt_rec, p);
    }
    return i;
}

static void iso9660_pt_fill(
        uint32_t i,
        iso9660_pt_rec_t *rec,
        void *p)
{
    iso9660_fs_t *self = p;

    if (self)
        self->pt_ptrs[i] = rec;
}

static int iso9660_name_compare_ascii(
        void const *name, size_t name_len,
        char const *find, size_t find_len)
{
    char const *chk = name;
    char const *find_limit = memrchr(find, '.', find_len);
    char const *chk_limit = memrchr(chk, '.', name_len);
    char const *find_end = find + find_len;
    char const *chk_end = memrchr(chk, ';', name_len);

    // ISO9660 uses a goofy DOS-like algorithm where the
    // shorter name is space-padded to the length of the
    // longer name, and the extension is space-padded
    // to the length of the longer extension

    int cmp = 0;

    for (int pass = 0; pass < 2; ++pass) {
        while (find < find_limit || chk < chk_limit) {
            int key_codepoint = find < find_limit
                    ? utf8_to_ucs4(find, &find)
                    : ' ';
            int chk_codepoint = chk < chk_limit
                    ? *chk
                    : ' ';

            cmp = chk_codepoint - key_codepoint;

            if (cmp)
                return cmp;
        }

        find_limit = find_end;
        chk_limit = chk_end;
    }

    return cmp;
}

static int iso9660_name_compare_utf16be(
        void const *name, size_t name_len,
        char const *find, size_t find_len)
{
    char const *find_end = find + find_len;
    uint16_t const *chk = name;
    uint16_t const *chk_end = chk + (name_len >> 1);

    int cmp = 0;
    while (find < find_end || chk < chk_end) {
        int key_codepoint = utf8_to_ucs4(find, &find);
        int chk_codepoint = utf16be_to_ucs4(chk, &chk);

        cmp = key_codepoint - chk_codepoint;

        if (cmp)
            break;
    }
    return cmp;
}

static int iso9660_lookup_path_cmp_ascii(
        void const *v, void const *k, void *s)
{
    (void)s;
    iso9660_pt_rec_t const *rec = *(iso9660_pt_rec_t const **)v;
    iso9660_path_key_t const *key = k;

    if (rec->parent_dn != key->parent)
        return key->parent - rec->parent_dn;

    size_t name_len = (rec->di_len - offsetof(
                iso9660_pt_rec_t, name));

    return iso9660_name_compare_ascii(
                rec->name, name_len,
                key->name, key->len);
}

static int iso9660_lookup_path_cmp_utf16be(
        void const *v, void const *k, void *s)
{
    (void)s;
    iso9660_pt_rec_t const *rec = *(iso9660_pt_rec_t const**)v;
    iso9660_path_key_t const *key = k;

    if (rec->parent_dn != key->parent)
        return key->parent - rec->parent_dn;

    return iso9660_name_compare_utf16be(
                rec->name, rec->di_len,
                key->name, key->len);
}

static iso9660_pt_rec_t *iso9660_lookup_path(
        iso9660_fs_t *self, char const *path, int path_len)
{
    iso9660_path_key_t key;

    if (path_len < 0)
        path_len = strlen(path);

    key.parent = 1;
    key.name = path;

    intptr_t match = -1;
    int done;
    do {
        char *sep = memchr(key.name, '/', path_len);
        done = !sep;
        if (!done)
            key.len = sep - key.name;
        else
            key.len = (path + path_len) - key.name;

        match = binary_search(
                    self->pt_ptrs, self->pt_count,
                    sizeof(*self->pt_ptrs), &key,
                    self->lookup_path_cmp, self, 1);

        if (match < 0)
            return 0;

        key.parent = match + 1;
        key.name += key.len + 1;
    } while(!done);

    return match >= 0 ? self->pt_ptrs[match] : 0;
}

static void *iso9660_lookup_sector(iso9660_fs_t *self, uint64_t lba)
{
    return self->mm_dev + (lba << self->sector_shift);
}

static size_t iso9660_next_dirent(iso9660_fs_t *self,
                                  iso9660_dir_ent_t *dir, size_t ofs)
{
    iso9660_dir_ent_t *de = (void*)((char*)dir + ofs);
    if (de->len != 0)
        return ofs + ((de->len + 1) & -2);
    return (ofs + (self->sector_size - 1)) & -(int)self->sector_size;
}

static iso9660_dir_ent_t *iso9660_lookup_dirent(
        iso9660_fs_t *self, char const *pathname)
{
    // Get length of whole path
    size_t pathname_len = strlen(pathname);

    // Find last path separator
    char const *sep = memrchr(pathname, '/', pathname_len);

    size_t path_len;
    size_t name_len;
    char const *name;

    if (sep) {
        path_len = sep - pathname;
        name = sep + 1;
        name_len = pathname_len - path_len - 1;
    } else {
        path_len = 0;
        name = pathname;
        name_len = pathname_len;
    }

    iso9660_pt_rec_t *pt_rec;

    if (path_len > 0) {
        pt_rec = iso9660_lookup_path(
                    self, pathname, path_len);
    } else {
        pt_rec = self->pt_ptrs[0];
    }

    iso9660_dir_ent_t *dir = iso9660_lookup_sector(
                self,
                iso9660_pt_rec_lba(pt_rec));

    if (!dir)
        return 0;

    size_t dir_len = iso9660_dirent_size(dir);

    iso9660_dir_ent_t *result = 0;
    for (size_t ofs = 0; ofs < dir_len;
         ofs = iso9660_next_dirent(self, dir, ofs)) {
        iso9660_dir_ent_t *de = (void*)((char*)dir + ofs);

        int cmp = self->name_compare(
                    de->name, de->filename_len,
                    name, name_len);
        if (cmp == 0) {
            result = de;
            break;
        }
    }

    return result;
}

static int iso9660_mm_fault_handler(
        void *dev, void *addr,
        uint64_t offset, uint64_t length)
{
    FS_DEV_PTR(dev);

    uint32_t sector_offset = (offset >> self->sector_shift);
    uint64_t lba = self->lba_st + sector_offset;

    printdbg("Demand paging LBA %ld at addr %p\n", lba, addr);

    return self->drive->vtbl->read_blocks(
                self->drive, addr, length >> self->sector_shift, lba);
}

//
// Startup and shutdown

static void* iso9660_mount(fs_init_info_t *conn)
{
    if (iso9660_mount_count == 0) {
        pool_create(&iso9660_handles, sizeof(iso9660_handle_t), 512);
    }

    iso9660_fs_t *self = iso9660_mounts + iso9660_mount_count++;

    self->vtbl = &iso9660_fs_device_vtbl;

    self->drive = conn->drive;

    self->sector_size = self->drive->vtbl->info(
                self->drive, STORAGE_INFO_BLOCKSIZE);

    self->sector_shift = bit_log2_n_32(self->sector_size);
    self->block_shift = 11 - self->sector_shift;
    self->block_size = self->sector_size << self->block_size;

    iso9660_pvd_t pvd;
    uint32_t best_ofs = 0;

    self->name_convert = iso9660_name_to_ascii;
    self->lookup_path_cmp = iso9660_lookup_path_cmp_ascii;
    self->name_compare = iso9660_name_compare_ascii;
    self->name_copy = iso9660_name_copy_ascii;

    for (uint32_t ofs = 0; ofs < 4; ++ofs) {
        // Read logical block 16
        self->drive->vtbl->read_blocks(
                    self->drive, &pvd,
                    1 << self->block_size,
                    (16 + ofs) << self->block_shift);

        if (pvd.type_code == 2) {
            // Prefer joliet pvd
            self->name_convert = iso9660_name_to_utf16be;
            self->lookup_path_cmp = iso9660_lookup_path_cmp_utf16be;
            self->name_compare = iso9660_name_compare_utf16be;
            self->name_copy = iso9660_name_copy_utf16be;
            best_ofs = ofs;
            break;
        }
    }

    if (best_ofs == 0) {
        // We didn't find Joliet PVD, reread first one
        self->drive->vtbl->read_blocks(
                    self->drive, &pvd,
                    1 << self->block_size,
                    16 << self->block_shift);
    }

    self->root_lba = iso9660_dirent_lba(&pvd.root_dirent);

    self->root_bytes = iso9660_dirent_size(&pvd.root_dirent);

    self->pt_lba = pvd.path_table_le_lba;
    self->pt_bytes = pvd.path_table_bytes.le;

    self->pt_alloc_size = iso9660_round_up(
                self->pt_bytes, self->sector_shift);

    self->pt = mmap(0, self->pt_alloc_size,
                    PROT_READ | PROT_WRITE,
                    MAP_POPULATE, -1, 0);

    self->drive->vtbl->read_blocks(
                self->drive, self->pt,
                self->pt_alloc_size >> self->sector_shift,
                self->pt_lba);

    // Count the path table entries
    self->pt_count = iso9660_walk_pt(self, 0, 0);

    // Allocate path table entry pointer array
    self->pt_ptrs = mmap(0, sizeof(*self->pt_ptrs) *
                         self->pt_count,
                         PROT_READ | PROT_WRITE, 0, -1, 0);

    // Populate path table entry pointer array
    iso9660_walk_pt(self, iso9660_pt_fill, self);

    self->lba_st = conn->part_st;
    self->lba_len = conn->part_len;

    self->mm_dev = mmap_register_device(
                self, self->block_size, conn->part_len,
                PROT_READ, iso9660_mm_fault_handler);

    iso9660_lookup_path(self, "usr/include", -1);

    iso9660_dir_ent_t *de = iso9660_lookup_dirent(self, "root/hello.txt");
    if (de) {
        char *content = iso9660_lookup_sector(
                    self, iso9660_dirent_lba(de));
        printdbg("%*s\n", de->size_lo_le, content);
    }

    return self;
}

static void iso9660_unmount(fs_base_t *dev)
{
    FS_DEV_PTR(dev);

    munmap(self->pt, self->pt_bytes);
    self->pt = 0;

    munmap(self->pt_ptrs, sizeof(*self->pt_ptrs) * self->pt_count);
    self->pt_ptrs = 0;
}

//
// Read directory entry information

static int iso9660_getattr(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_stat_t* stbuf)
{
    FS_DEV_PTR(dev);

    iso9660_dir_ent_t *de = iso9660_lookup_dirent(self, path);

    // Device ID of device containing file.
    stbuf->st_dev = 0;

    // File serial number.
    stbuf->st_ino = 0;

    // Mode of file (see below)
    stbuf->st_mode = 0;

    // Number of hard links to the file
    stbuf->st_nlink = 0;

    // User ID of file
    stbuf->st_uid = 0;

    // Group ID of file
    stbuf->st_gid = 0;

    // Device ID (if file is character or block special)
    stbuf->st_rdev = 0;

    // For regular files, the file size in bytes.
    // For symbolic links, the length in bytes of the
    // pathname contained in the symbolic link.
    // For other file types, the use of this field is
    // unspecified.
    stbuf->st_size = iso9660_dirent_size(de);

    // Last data modification timestamp
    stbuf->st_mtime = 0;
    // Last file status change timestamp
    stbuf->st_ctime = 0;

    // A file system-specific preferred I/O block size
    // for this object. In some file system types, this
    // may vary from file to file.
    stbuf->st_blksize = 0;

    // Number of blocks allocated for this object.
    stbuf->st_blocks = iso9660_dirent_size(de)
            >> (self->sector_shift + self->block_shift);

    return 0;
}

static int iso9660_access(
        fs_base_t *dev,
        fs_cpath_t path,
        int mask)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mask;
    return 0;
}

static int iso9660_readlink(
        fs_base_t *dev,
        fs_cpath_t path,
        char* buf,
        size_t size)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)buf;
    (void)size;
    return 0;
}

//
// Scan directories

static int iso9660_opendir(fs_base_t *dev,
                           fs_file_info_t **fi,
                           fs_cpath_t path)
{
    FS_DEV_PTR(dev);

    iso9660_pt_rec_t *pt = iso9660_lookup_path(self, path, -1);

    iso9660_dir_handle_t *dir = pool_alloc(&iso9660_handles);
    dir->fs = self;
    dir->dirent = iso9660_lookup_sector(
                self, iso9660_pt_rec_lba(pt));
    dir->content = (void*)dir->dirent;
    *fi = dir;

    return 0;
}

static ssize_t iso9660_readdir(fs_base_t *dev,
                               fs_file_info_t *fi,
                               dirent_t *buf,
                               off_t offset)
{
    FS_DEV_PTR(dev);

    iso9660_dir_handle_t *dir = fi;

    ssize_t dir_size = (ssize_t)iso9660_dirent_size(dir->dirent);

    if (offset >= dir_size)
        return 0;

    size_t orig_offset = offset;

    iso9660_dir_ent_t *fe = (void*)dir->content;
    iso9660_dir_ent_t *de = (void*)(dir->content + offset);

    if (de->len == 0) {
        // Next entry would cross a sector boundary
        offset = iso9660_next_dirent(self, fe, offset);
        de = (void*)(dir->content + offset);

        if (offset >= dir_size)
            return 0;
    }

    self->name_copy(buf->d_name, de->name, de->filename_len);

    offset = iso9660_next_dirent(self, fe, offset);

    return offset - orig_offset;
}

static int iso9660_releasedir(fs_base_t *dev,
                              fs_file_info_t *fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
    pool_free(&iso9660_handles, fi);
    return 0;
}


//
// Modify directories

static int iso9660_mknod(fs_base_t *dev,
                         fs_cpath_t path,
                         fs_mode_t mode,
                         fs_dev_t rdev)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mode;
    (void)rdev;
    // Fail, read only
    return -1;
}

static int iso9660_mkdir(fs_base_t *dev,
                         fs_cpath_t path,
                         fs_mode_t mode)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mode;
    // Fail, read only
    return -1;
}

static int iso9660_rmdir(fs_base_t *dev,
                         fs_cpath_t path)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    // Fail, read only
    return -1;
}

static int iso9660_symlink(
        fs_base_t *dev,
        fs_cpath_t to,
        fs_cpath_t from)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)to;
    (void)from;
    // Fail, read only
    return -1;
}

static int iso9660_rename(
        fs_base_t *dev,
        fs_cpath_t from,
        fs_cpath_t to)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)from;
    (void)to;
    // Fail, read only
    return -1;
}

static int iso9660_link(
        fs_base_t *dev,
        fs_cpath_t from,
        fs_cpath_t to)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)from;
    (void)to;
    // Fail, read only
    return -1;
}

static int iso9660_unlink(
        fs_base_t *dev,
        fs_cpath_t path)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    // Fail, read only
    return -1;
}

//
// Modify directory entries

static int iso9660_chmod(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_mode_t mode)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mode;
    // Fail, read only
    return -1;
}

static int iso9660_chown(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_uid_t uid,
        fs_gid_t gid)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)uid;
    (void)gid;
    // Fail, read only
    return -1;
}

static int iso9660_truncate(
        fs_base_t *dev,
        fs_cpath_t path,
        off_t size)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)size;
    // Fail, read only
    return -1;
}

static int iso9660_utimens(
        fs_base_t *dev,
        fs_cpath_t path,
        const fs_timespec_t *ts)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)ts;
    // Fail, read only
    return -1;
}


//
// Open/close files

static int iso9660_open(fs_base_t *dev,
                        fs_file_info_t **fi,
                        fs_cpath_t path)
{
    FS_DEV_PTR(dev);

    iso9660_file_handle_t *file = pool_alloc(&iso9660_handles);
    *fi = file;

    file->dirent = iso9660_lookup_dirent(self, path);
    file->content = iso9660_lookup_sector(
                self, iso9660_dirent_lba(file->dirent));

    return 0;
}

static int iso9660_release(fs_base_t *dev,
                           fs_file_info_t *fi)
{
    FS_DEV_PTR_UNUSED(dev);
    pool_free(&iso9660_handles, fi);
    return 0;
}


//
// Read/write files

static ssize_t iso9660_read(fs_base_t *dev,
                            fs_file_info_t *fi,
                            char *buf,
                            size_t size,
                            off_t offset)
{
    FS_DEV_PTR_UNUSED(dev);

    iso9660_file_handle_t *file = fi;

    size_t file_size = iso9660_dirent_size(file->dirent);

    if (offset < 0)
        return -1;

    if ((size_t)offset >= file_size)
        return 0;

    if (offset + size > file_size)
        size = file_size - offset;

    memcpy(buf, file->content + offset, size);

    return size;
}

static ssize_t iso9660_write(fs_base_t *dev,
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
    // Fail, read only
    return -1;
}

static int iso9660_ftruncate(fs_base_t *dev,
                             fs_file_info_t *fi,
                             off_t offset)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)offset;
    (void)fi;
    // Fail, read only
    return -1;
}

//
// Query open file

static int iso9660_fstat(fs_base_t *dev,
                         fs_file_info_t *fi,
                         fs_stat_t *st)
{
    FS_DEV_PTR_UNUSED(dev);

    iso9660_file_handle_t *file = fi;

    size_t file_size = iso9660_dirent_size(file->dirent);

    memset(st, 0, sizeof(*st));
    // FIXME: fill in more fields
    st->st_size = file_size;

    return 0;
}

//
// Sync files and directories and flush buffers

static int iso9660_fsync(fs_base_t *dev,
                         fs_file_info_t *fi,
                         int isdatasync)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)isdatasync;
    (void)fi;
    // Read only device, do nothing
    return 0;
}

static int iso9660_fsyncdir(fs_base_t *dev,
                            fs_file_info_t *fi,
                            int isdatasync)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)isdatasync;
    (void)fi;
    // Ignore, read only
    return 0;
}

static int iso9660_flush(fs_base_t *dev,
                         fs_file_info_t *fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
    // Do nothing, read only
    return 0;
}

//
// Get filesystem information

static int iso9660_statfs(fs_base_t *dev,
                          fs_statvfs_t* stbuf)
{
    FS_DEV_PTR(dev);

    // Filesystem block size
    stbuf->f_bsize = 1UL << (self->sector_shift + self->block_shift);

    // Fragment size
    stbuf->f_frsize = stbuf->f_bsize;

    // Size of filesystem in f_frsize units
    stbuf->f_blocks = self->lba_len;

    // free blocks
    stbuf->f_bfree = 0;

    // free blocks for unprivileged users
    stbuf->f_bavail = 0;

    // inodes
    stbuf->f_files = 0;

    // Free inodes
    stbuf->f_ffree = 0;

    // Free inodes for unprivileged users
    stbuf->f_favail = 0;

    // Filesystem ID
    stbuf->f_fsid = 0xCD;

    // Mount flags
    stbuf->f_flag = 1;//ST_RDONLY;

    // Maximum filename length
    stbuf->f_namemax = ISO9660_MAX_NAME;

    return 0;
}

//
// lock/unlock file

static int iso9660_lock(fs_base_t *dev,
                        fs_file_info_t *fi,
                        int cmd,
                        fs_flock_t* locks)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
    (void)cmd;
    (void)locks;
    return 0;
}

//
// Get block map

static int iso9660_bmap(
        fs_base_t *dev,
        fs_cpath_t path,
        size_t blocksize,
        uint64_t* blockno)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)blocksize;
    (void)blockno;
    // No idea what this does
    return -1;
}

//
// Read/Write/Enumerate extended attributes

static int iso9660_setxattr(
        fs_base_t *dev,
        fs_cpath_t path,
        char const* name,
        char const* value,
        size_t size,
        int flags)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    // Fail, read only
    return -1;
}

static int iso9660_getxattr(
        fs_base_t *dev,
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
    return 0;
}

static int iso9660_listxattr(
        fs_base_t *dev,
        fs_cpath_t path,
        char const* list,
        size_t size)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)list;
    (void)size;
    return 0;
}

//
// ioctl API

static int iso9660_ioctl(fs_base_t *dev,
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
    return 0;
}

//
//

static int iso9660_poll(fs_base_t *dev,
                        fs_file_info_t *fi,
                        fs_pollhandle_t* ph,
                        unsigned* reventsp)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
    (void)ph;
    (void)reventsp;
    return 0;
}

REGISTER_fs_DEVICE(iso9660);
