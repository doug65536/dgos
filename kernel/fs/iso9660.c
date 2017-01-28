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

struct iso9660_fs_t {
    fs_vtbl_t *vtbl;

    storage_dev_base_t *drive;

    // Root
    uint32_t root_lba;
    uint32_t root_bytes;

    // Path table
    uint32_t pt_lba;
    uint32_t pt_bytes;

    int (*convert_name)(void *ascii_buf,
                        char const *utf8);

    // Path table and path table lookup table
    iso9660_pt_rec_t *pt;
    iso9660_pt_rec_t **pt_ptrs;

    uint32_t pt_alloc_size;
    uint32_t pt_count;

    // From the drive
    uint32_t sector_size;

    // >= 2KB
    uint32_t block_size;

    // log2(sector_size)
    uint8_t sector_shift;

    // log2(num_sectors_per_block)
    uint8_t block_shift;
};

iso9660_fs_t iso9660_mounts[16];
unsigned iso9660_mount_count;

//
// Startup and shutdown

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
                          (offsetof(iso9660_pt_rec_t, name) +
                          pt_rec->di_len + 1 & -2))) {
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

static void* iso9660_mount(fs_init_info_t *conn)
{
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

    self->convert_name = iso9660_name_to_ascii;

    for (uint32_t ofs = 0; ofs < 4; ++ofs) {
        // Read logical block 16
        self->drive->vtbl->read_blocks(
                    self->drive, &pvd,
                    1 << self->block_size,
                    (16 + ofs) << self->block_shift);

        if (pvd.type_code == 2) {
            // Prefer joliet pvd
            self->convert_name = iso9660_name_to_utf16be;
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

    self->root_lba = pvd.root_dirent.lba_lo_le |
            (pvd.root_dirent.lba_hi_le << 16);

    self->root_bytes = pvd.root_dirent.size_lo_le |
            (pvd.root_dirent.size_hi_le << 16);

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
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)stbuf;
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

static int iso9660_opendir(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_file_info_t* fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)fi;
    return 0;
}

static int iso9660_readdir(
        fs_base_t *dev,
        fs_cpath_t path,
        void* buf,
        off_t offset,
        fs_file_info_t* fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)buf;
    (void)offset;
    (void)fi;
    return 0;
}

static int iso9660_releasedir(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_file_info_t *fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)fi;
    return 0;
}


//
// Modify directories

static int iso9660_mknod(
        fs_base_t *dev,
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

static int iso9660_mkdir(
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

static int iso9660_rmdir(
        fs_base_t *dev,
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
    return 0;
}

static int iso9660_truncate(
        fs_base_t *dev,
        fs_cpath_t path,
        off_t size)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)size;
    return 0;
}

static int iso9660_utimens(
        fs_base_t *dev,
        fs_cpath_t path,
        const fs_timespec_t *ts)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)ts;
    return 0;
}


//
// Open/close files

static int iso9660_open(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_file_info_t* fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)fi;
    return 0;
}

static int iso9660_release(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_file_info_t *fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)fi;
    return 0;
}


//
// Read/write files

static int iso9660_read(
        fs_base_t *dev,
        fs_cpath_t path,
        char *buf,
        size_t size,
        off_t offset,
        fs_file_info_t* fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    return 0;
}

static int iso9660_write(
        fs_base_t *dev,
        fs_cpath_t path,
        char *buf,
        size_t size,
        off_t offset,
        fs_file_info_t* fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    // Fail, read only
    return -1;
}


//
// Sync files and directories and flush buffers

static int iso9660_fsync(
        fs_base_t *dev,
        fs_cpath_t path,
        int isdatasync,
        fs_file_info_t* fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)isdatasync;
    (void)fi;
    return 0;
}

static int iso9660_fsyncdir(
        fs_base_t *dev,
        fs_cpath_t path,
        int isdatasync,
        fs_file_info_t* fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)isdatasync;
    (void)fi;
    return 0;
}

static int iso9660_flush(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_file_info_t* fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)fi;
    return 0;
}

//
// Get filesystem information

static int iso9660_statfs(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_statvfs_t* stbuf)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)stbuf;
    return 0;
}

//
// lock/unlock file

static int iso9660_lock(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_file_info_t* fi,
        int cmd,
        fs_flock_t* locks)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
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
    return 0;
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

static int iso9660_ioctl(
        fs_base_t *dev,
        fs_cpath_t path,
        int cmd,
        void* arg,
        fs_file_info_t* fi,
        unsigned int flags,
        void* data)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)cmd;
    (void)arg;
    (void)fi;
    (void)flags;
    (void)data;
    return 0;
}

//
//

static int iso9660_poll(
        fs_base_t *dev,
        fs_cpath_t path,
        fs_file_info_t* fi,
        fs_pollhandle_t* ph,
        unsigned* reventsp)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)fi;
    (void)ph;
    (void)reventsp;
    return 0;
}

REGISTER_fs_DEVICE(iso9660);
