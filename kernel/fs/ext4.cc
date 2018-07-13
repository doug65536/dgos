#include "dev_storage.h"

#include "bitop.h"
#include "mm.h"
#include "printk.h"
#include "vector.h"
#include "inttypes.h"

class ext4_fs_t final : public fs_base_t {
    FS_BASE_IMPL

    friend class ext4_factory_t;

    typedef uint8_t u8;
    typedef uint16_t le16;
    typedef uint32_t le32;
    typedef uint64_t le64;

    static constexpr le16 magic = 0xEF53;

    struct superblock_t {
        enum struct state_t : le16 {
            CLEAN   = 0x0001,
            ERRORS  = 0x0002,
            ORPHANS = 0x0004
        };

        enum struct creator_os_t : le32 {
            LINUX_OS,
            HURD_OS,
            MASIX_OS,
            FREEBSD_OS,
            LITES_OS
        };

        enum struct rev_level_t : le32 {
            ORIGINAL,
            V2_DYNAMIC_INODES
        };

        enum struct feature_compat_t : le32 {
            DIR_PREALLOC     = 0x1,
            IMAGIC_INODES    = 0x2,
            HAS_JOURNAL      = 0x4,
            EXT_ATTR         = 0x8,
            RESIZE_INODE     = 0x10,
            DIR_INDEX        = 0x20,
            LAZY_BG          = 0x40,
            EXCLUDE_INODE    = 0x80,
            EXCLUDE_BITMAP   = 0x100
        };

        enum struct feature_incompat_t : le32 {
            COMPRESSION        = 0x1,
            FILETYPE           = 0x2,
            RECOVER            = 0x4,
            JOURNAL_DEV        = 0x8,
            META_BG            = 0x10,
            EXTENTS            = 0x40,
            IS64BIT            = 0x80,
            MMP                = 0x100,
            FLEX_BG            = 0x200,
            EA_INODE           = 0x400,
            DIRDATA            = 0x1000,
            BG_USE_META_CSUM   = 0x2000,
            LARGEDIR           = 0x4000,
            INLINE_DATA        = 0x8000
        };

        enum struct feature_ro_compat_t : le32 {
            SPARSE_SUPER = 0x1,
            LARGE_FILE = 0x2,
            BTREE_DIR = 0x4,
            HUGE_FILE = 0x8,
            GDT_CSUM = 0x10,
            DIR_NLINK = 0x20,
            EXTRA_ISIZE = 0x40,
            HAS_SNAPSHOT = 0x80,
            QUOTA = 0x100,
            BIGALLOC = 0x200,
            METADATA_CSUM = 0x400
        };

        enum struct hash_version_t : u8 {
            LEGACY,
            HALF_MD4,
            TEA,
            LEGACY_UNSIGNED,
            TEA_UNSIGNED
        };

        enum struct mount_opts_t : le32 {
            DEBUGGING = 0x0001,
            GID_FROM_PARENT_DIR = 0x0002,
            SUPPORT_USERSPACE_EA = 0x0004,
            SUPPORT_POSIX_ACL = 0x0008,
            DONT_SUPPORT_32BIT_UID = 0x0010,
            JOURNAL_DATA = 0x0020,
            ORDERED_DATA = 0x0040,
            UNORDERED_DATA = 0x0060,
            NOFLUSH = 0x0100,
            TRACK_METADATA = 0x0200,
            ENABLE_DISCARD = 0x0400,
            NO_DELAYED_ALLOC = 0x0800
        };

        enum struct misc_flags_t : le32 {
            SIGNED_DIR_HASH     = 0x1,
            UNSIGNED_DIR_HASH   = 0x2,
            TEST_FILESYSTEM     = 0x4
        };

        le32 s_inodes_count;
        le32 s_blocks_count_lo;
        le32 s_r_blocks_count_lo;
        le32 s_free_blocks_count_lo;
        le32 s_free_inodes_count;
        le32 s_first_data_block;
        le32 s_log_block_size;
        le32 s_log_cluster_size;
        le32 s_blocks_per_group;
        le32 s_obso_frags_per_group;
        le32 s_inodes_per_group;
        le32 s_mtime;
        le32 s_wtime;
        le16 s_mnt_count;
        le16 s_max_mnt_count;
        le16 s_magic;
        state_t s_state;
        le16 s_errors;
        le16 s_minor_rev_level;
        le32 s_lastcheck;
        le32 s_checkinterval;
        creator_os_t s_creator_os;
        rev_level_t s_rev_level;
        le16 s_def_resuid;
        le16 s_def_resgid;

        le32 s_first_ino;
        le16 s_inode_size;
        le16 s_block_group_nr;
        feature_compat_t s_feature_compat;
        feature_incompat_t s_feature_incompat;
        feature_ro_compat_t s_feature_ro_compat;
        u8 s_uuid[16];
        char s_volume_name[16];
        char s_last_mounted[64];
        le32 s_algorithm_usage_bitmap;
        u8 s_prealloc_blocks;
        u8 s_prealloc_dir_blocks;
        le16 s_reserved_gdt_blocks;

        //
        // Journal information

        u8 s_journal_uuid[16];
        le32 s_journam_inum;
        le32 s_journal_dev;
        le32 s_last_orphan;
        le32 s_hash_seed[4];
        hash_version_t s_def_hash_version;
        u8 s_jnl_backup_type;
        le16 s_desc_size;
        mount_opts_t s_default_mount_opts;
        le32 s_first_meta_bg;
        le32 s_mkfs_time;
        le32 s_jnl_blocks[17];

        //
        // 64 bit support

        le32 s_blocks_count_hi;
        le32 s_r_blocks_count_hi;
        le32 s_free_blocks_count_hi;

        le16 s_min_extra_isize;
        le16 s_want_extra_isize;
        misc_flags_t s_flags;
        le16 s_raid_stride;
        le16 s_mmp_interval;
        le64 s_mmp_block;
        le32 s_raid_stripe_width;
        u8 s_log_groups_per_flex;
        u8 s_reserved_char_pad;
        le16 s_reserved_pad;
        le64 s_kbytes_written;

        //
        // Snapshot information

        le32 s_snapshot_inum;
        le32 s_snapshot_id;
        le64 s_snapshot_r_blocks_count;
        le32 s_snapshot_list;

        //
        // Error information

        le32 s_error_count;

        le32 s_first_error_time;
        le32 s_first_error_ino;
        le64 s_first_error_block;
        u8 s_first_error_func[32];
        le32 s_first_error_line;

        le32 s_last_error_time;
        le32 s_last_error_ino;
        le32 s_last_error_line;
        le64 s_last_error_block;
        u8 s_last_error_func[32];

        u8 s_mount_opts[64];
        le32 s_usr_quota_inum;
        le32 s_grp_quota_inum;
        le32 s_overhead_blocks;
        le32 s_reserved[108];
        le32 s_checksum;
    } _packed;

    C_ASSERT(offsetof(superblock_t, s_default_mount_opts) == 0x100);
    C_ASSERT(offsetof(superblock_t, s_snapshot_inum) == 0x180);
    C_ASSERT(offsetof(superblock_t, s_snapshot_list) == 0x190);
    C_ASSERT(offsetof(superblock_t, s_first_error_func) == 0x1A8);
    C_ASSERT(offsetof(superblock_t, s_mount_opts) == 0x200);
    C_ASSERT(sizeof(superblock_t) == 0x400);

    friend constexpr bool enable_bitwise(superblock_t::state_t);
    friend constexpr bool enable_bitwise(superblock_t::feature_compat_t);
    friend constexpr bool enable_bitwise(superblock_t::feature_incompat_t);
    friend constexpr bool enable_bitwise(superblock_t::feature_ro_compat_t);
    friend constexpr bool enable_bitwise(superblock_t::mount_opts_t);
    friend constexpr bool enable_bitwise(superblock_t::misc_flags_t);

    bool mount(fs_init_info_t *conn);

    //
    // Internals

    static int mm_fault_handler(void *dev, void *addr,
                                uint64_t offset, uint64_t length,
                                bool read, bool flush);
    int mm_fault_handler(void *addr, uint64_t offset, uint64_t length,
                         bool read, bool flush);

    storage_dev_base_t *drive;
    uint64_t part_st;
    uint64_t part_len;

    static constexpr uint8_t sector_shift = 9;
    static constexpr uint8_t block_shift = 3;
    static constexpr uint32_t sector_size = 1U << (sector_shift);
    static constexpr uint32_t block_size = sector_size << block_shift;

    char *mm_dev;
};

class ext4_factory_t : public fs_factory_t {
    fs_base_t *mount(fs_init_info_t *conn) override;
};

static vector<ext4_fs_t*> ext4_mounts;

// ---------------------------------------------------------------------------
// Internal implementation
// ---------------------------------------------------------------------------

int ext4_fs_t::mm_fault_handler(
        void *dev, void *addr, uint64_t offset, uint64_t length,
        bool read, bool flush)
{
    FS_DEV_PTR(ext4_fs_t, dev);
    return self->mm_fault_handler(addr, offset, length, read, flush);
}

int ext4_fs_t::mm_fault_handler(
        void *addr, uint64_t offset, uint64_t length, bool read, bool flush)
{
    uint64_t sector_offset = (offset >> sector_shift);
    uint64_t lba = part_st + sector_offset;

    if (likely(read)) {
        printdbg("Demand paging LBA %" PRId64 " at addr %p\n", lba, addr);

        return drive->read_blocks(addr, length >> sector_shift, lba);
    }

    printdbg("Writing back LBA %" PRId64 " at addr %p\n", lba, addr);
    int result = drive->write_blocks(addr, length >> sector_shift, lba, flush);

    return result;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

//
// Startup and shutdown

fs_base_t *ext4_factory_t::mount(fs_init_info_t *conn)
{
    unique_ptr<ext4_fs_t> self(new ext4_fs_t);
    if (self->mount(conn)) {
        ext4_mounts.push_back(self);
        return self.release();
    }

    return nullptr;
}

bool ext4_fs_t::mount(fs_init_info_t *conn)
{
    drive = conn->drive;
    part_st = conn->part_st;
    part_len = conn->part_len;

    mm_dev = (char*)mmap_register_device(
                this, block_size, conn->part_len,
                PROT_READ | PROT_WRITE, &ext4_fs_t::mm_fault_handler);

    if (!mm_dev)
        return false;

    return false;
}

void ext4_fs_t::unmount()
{
}

bool ext4_fs_t::is_boot() const
{
    return false;
}

//
// Read directory entry information

int ext4_fs_t::getattr(fs_cpath_t path, fs_stat_t* stbuf)
{
    (void)path;
    (void)stbuf;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::access(fs_cpath_t path, int mask)
{
    (void)path;
    (void)mask;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::readlink(fs_cpath_t path, char* buf, size_t size)
{
    (void)path;
    (void)buf;
    (void)size;
    return -int(errno_t::ENOSYS);
}

//
// Scan directories

int ext4_fs_t::opendir(fs_file_info_t **fi, fs_cpath_t path)
{
    (void)path;
    (void)fi;
    return -int(errno_t::ENOSYS);
}

ssize_t ext4_fs_t::readdir(fs_file_info_t *fi,
                                   dirent_t *buf, off_t offset)
{
    (void)buf;
    (void)offset;
    (void)fi;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::releasedir(fs_file_info_t *fi)
{
    (void)fi;
    return -int(errno_t::ENOSYS);
}


//
// Modify directories

int ext4_fs_t::mknod(fs_cpath_t path, fs_mode_t mode, fs_dev_t rdev)
{
    (void)path;
    (void)mode;
    (void)rdev;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::mkdir(fs_cpath_t path, fs_mode_t mode)
{
    (void)path;
    (void)mode;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::rmdir(fs_cpath_t path)
{
    (void)path;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::symlink(fs_cpath_t to, fs_cpath_t from)
{
    (void)to;
    (void)from;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::rename(fs_cpath_t from, fs_cpath_t to)
{
    (void)from;
    (void)to;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::link(fs_cpath_t from, fs_cpath_t to)
{
    (void)from;
    (void)to;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::unlink(fs_cpath_t path)
{
    (void)path;
    return -int(errno_t::ENOSYS);
}

//
// Modify directory entries

int ext4_fs_t::chmod(fs_cpath_t path, fs_mode_t mode)
{
    (void)path;
    (void)mode;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::chown(fs_cpath_t path, fs_uid_t uid, fs_gid_t gid)
{
    (void)path;
    (void)uid;
    (void)gid;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::truncate(fs_cpath_t path, off_t size)
{
    (void)path;
    (void)size;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::utimens(fs_cpath_t path, const fs_timespec_t *ts)
{
    (void)path;
    (void)ts;
    return -int(errno_t::ENOSYS);
}


//
// Open/close files

int ext4_fs_t::open(fs_file_info_t **fi,
                     fs_cpath_t path, int flags, mode_t mode)
{
    (void)fi;
    (void)path;
    (void)flags;
    (void)mode;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::release(fs_file_info_t *fi)
{
    (void)fi;
    return -int(errno_t::ENOSYS);
}


//
// Read/write files

ssize_t ext4_fs_t::read(fs_file_info_t *fi, char *buf,
                                size_t size, off_t offset)
{
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    return -int(errno_t::ENOSYS);
}

ssize_t ext4_fs_t::write(fs_file_info_t *fi, char const *buf,
                                 size_t size, off_t offset)
{
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::ftruncate(fs_file_info_t *fi, off_t offset)
{
    (void)offset;
    (void)fi;
    // Fail, read only
    return -1;
}

//
// Query open files

int ext4_fs_t::fstat(fs_file_info_t *fi, fs_stat_t *st)
{
    (void)fi;
    (void)st;
    return -int(errno_t::ENOSYS);
}

//
// Sync files and directories and flush buffers

int ext4_fs_t::fsync(fs_file_info_t *fi, int isdatasync)
{
    (void)isdatasync;
    (void)fi;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::fsyncdir(fs_file_info_t *fi, int isdatasync)
{
    (void)isdatasync;
    (void)fi;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::flush(fs_file_info_t *fi)
{
    (void)fi;
    return -int(errno_t::ENOSYS);
}

//
// Get filesystem information

int ext4_fs_t::statfs(fs_statvfs_t* stbuf)
{
    (void)stbuf;
    return -int(errno_t::ENOSYS);
}

//
// lock/unlock file

int ext4_fs_t::lock(
        fs_file_info_t *fi, int cmd, fs_flock_t* locks)
{
    (void)fi;
    (void)cmd;
    (void)locks;
    return -int(errno_t::ENOSYS);
}

//
// Get block map

int ext4_fs_t::bmap(
        fs_cpath_t path, size_t blocksize, uint64_t* blockno)
{
    (void)path;
    (void)blocksize;
    (void)blockno;
    return -int(errno_t::ENOSYS);
}

//
// Read/Write/Enumerate extended attributes

int ext4_fs_t::setxattr(
        fs_cpath_t path,
        char const* name, char const* value,
        size_t size, int flags)
{
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::getxattr(
        fs_cpath_t path,
        char const* name, char* value,
        size_t size)
{
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    return -int(errno_t::ENOSYS);
}

int ext4_fs_t::listxattr(
        fs_cpath_t path,
        char const* list, size_t size)
{
    (void)path;
    (void)list;
    (void)size;
    return -int(errno_t::ENOSYS);
}

//
// ioctl API

int ext4_fs_t::ioctl(
        fs_file_info_t *fi,
        int cmd,
        void* arg,
        unsigned int flags,
        void* data)
{
    (void)cmd;
    (void)arg;
    (void)fi;
    (void)flags;
    (void)data;
    return -int(errno_t::ENOSYS);
}

//
//

int ext4_fs_t::poll(fs_file_info_t *fi,
                            fs_pollhandle_t* ph,
                            unsigned* reventsp)
{
    (void)fi;
    (void)ph;
    (void)reventsp;
    return -int(errno_t::ENOSYS);
}

constexpr bool enable_bitwise(ext4_fs_t::superblock_t::state_t)
{
    return true;
}

constexpr bool enable_bitwise(ext4_fs_t::superblock_t::feature_compat_t)
{
    return true;
}

constexpr bool enable_bitwise(ext4_fs_t::superblock_t::feature_incompat_t)
{
    return true;
}

constexpr bool enable_bitwise(ext4_fs_t::superblock_t::feature_ro_compat_t)
{
    return true;
}

constexpr bool enable_bitwise(ext4_fs_t::superblock_t::mount_opts_t)
{
    return true;
}

constexpr bool enable_bitwise(ext4_fs_t::superblock_t::misc_flags_t)
{
    return true;
}
