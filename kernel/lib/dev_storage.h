#pragma once
#include "types.h"
#include "dirent.h"
#include "errno.h"
#include "cpu/atomic.h"
#include "device/iocp.h"
#include "vector.h"

// Storage device interface (IDE, AHCI, etc)

#include "dev_registration.h"

// Filesystem I/O code builds a list of these to do burst I/O
struct disk_vec_t {
    // Start LBA of range
    uint64_t lba;

    // Number of contiguous sectors
    uint32_t count;

    // Offset into first sector to transfer
    // If this is nonzero, count is guaranteed to be 1
    uint16_t sector_ofs;

    // Number of bytes to transfer.
    // If count is nonzero, this field is ignored, and count is the number
    // of whole sectors to transfer.
    uint16_t byte_count;
};

struct disk_io_plan_t {
    void *dest;
    disk_vec_t *vec;
    uint16_t count;
    uint16_t capacity;
    uint8_t log2_sector_size;

    disk_io_plan_t(void *dest, uint8_t log2_sector_size);
    disk_io_plan_t(disk_io_plan_t const&) = delete;
    disk_io_plan_t() = delete;
    ~disk_io_plan_t();

    bool add(uint32_t lba, uint16_t sector_count,
             uint16_t sector_ofs, uint16_t byte_count);
};

//
// Storage Device (hard drive, CDROM, etc)

enum storage_dev_info_t : uint32_t {
    STORAGE_INFO_NONE = 0,
    STORAGE_INFO_BLOCKSIZE,
    STORAGE_INFO_HAVE_TRIM,
    STORAGE_INFO_NAME
};

struct EXPORT storage_dev_base_t {
    virtual ~storage_dev_base_t() = 0;

    // Startup/shutdown
    virtual void cleanup_dev() = 0;

    //
    // Asynchronous I/O plan

    //virtual errno_t io(disk_io_plan_t *plan);

    //
    // Asynchronous I/O

    virtual errno_t read_async(void *data, int64_t count,
                               uint64_t lba, iocp_t *iocp) = 0;

    virtual errno_t write_async(void const *data, int64_t count,
                                uint64_t lba, bool fua, iocp_t *iocp) = 0;

    virtual errno_t trim_async(int64_t count, uint64_t lba, iocp_t *iocp) = 0;

    virtual errno_t flush_async(iocp_t *iocp) = 0;

    // Synchronous wrappers

    int read_blocks(void *data, int64_t count, uint64_t lba);

    int write_blocks(void const *data, int64_t count, uint64_t lba, bool fua);

    virtual int64_t trim_blocks(int64_t count, uint64_t lba);

    virtual int flush();

    virtual long info(storage_dev_info_t key) = 0;
};

#define STORAGE_DEV_IMPL                                \
    void cleanup_dev() override final;                  \
                                                        \
    errno_t read_async(                                 \
            void *data, int64_t count,                  \
            uint64_t lba,                               \
            iocp_t *iocp) override final;               \
                                                        \
    errno_t write_async(                                \
            void const *data, int64_t count,            \
            uint64_t lba, bool fua,                     \
            iocp_t *iocp) override final;               \
                                                        \
    errno_t flush_async(                                \
            iocp_t *iocp) override final;               \
                                                        \
    errno_t trim_async(                                 \
            int64_t count,                              \
            uint64_t lba,                               \
            iocp_t *iocp) override final;               \
                                                        \
    long info(storage_dev_info_t key) final override;

//
// Storage Interface (IDE, AHCI, etc)

struct storage_if_base_t;

struct EXPORT storage_if_factory_t {
    storage_if_factory_t(char const *factory_name);
    virtual ~storage_if_factory_t();

    void register_factory();

    virtual std::vector<storage_if_base_t *> detect(void) = 0;
    char const * const name;
};

#pragma GCC visibility push(default)
struct EXPORT storage_if_base_t {
    virtual ~storage_if_base_t() = 0;
    virtual void cleanup_if() = 0;
    virtual std::vector<storage_dev_base_t*> detect_devices() = 0;
};
#pragma GCC visibility pop

#define STORAGE_IF_IMPL                         \
    void cleanup_if() override final;           \
    std::vector<storage_dev_base_t*> detect_devices() override final;

#define STORAGE_REGISTER_FACTORY(name) \
    REGISTER_CALLOUT(& name##_factory_t::register_factory, \
        & name##_factory, callout_type_t::late_dev, "000")

bool storage_if_unregister_factory(storage_if_factory_t *factory);
void storage_if_register_factory(storage_if_factory_t *factory);

typedef int dev_t;

size_t storage_dev_count();
storage_dev_base_t *storage_dev_open(dev_t dev);
void storage_dev_close(storage_dev_base_t *dev);

template<typename T>
bool unregister_factory(std::vector<T*> &factories,
                        T *factory)
{
    typename std::vector<T*>::iterator pos =
            std::find(factories.begin(),
                      factories.end(), factory);

    if (unlikely(pos == factories.end()))
        return false;

    factories.erase(pos);

    return true;
}


//
// Filesystem (FAT32, etc)

struct fs_vtbl_t;

struct fs_base_t;

typedef char const *fs_cpath_t;

typedef uint16_t fs_mode_t;
typedef uint32_t fs_uid_t;
typedef uint32_t fs_gid_t;

struct fs_init_info_t;

struct fs_stat_t;
class fs_file_info_t
{
public:
    virtual ~fs_file_info_t() {}
    virtual ino_t get_inode() const = 0;
};

struct fs_statvfs_t;

struct fs_flock_t;

typedef uint64_t fs_timespec_t;

typedef uint64_t fs_dev_t;

struct fs_pollhandle_t;

struct fs_init_info_t {
    storage_dev_base_t *drive;

    // Partition start LBA
    uint64_t part_st;
    uint64_t part_len;
};

typedef uint64_t ino_t;
typedef uint16_t mode_t;
typedef uint32_t nlink_t;
typedef int32_t uid_t;
typedef int32_t gid_t;
typedef uint64_t blksize_t;
typedef uint64_t blkcnt_t;
typedef uint64_t time_t;

struct fs_stat_t {
    dev_t     st_dev;       /* ID of device containing file */
    ino_t     st_ino;       /* inode number */
    mode_t    st_mode;      /* protection */
    nlink_t   st_nlink;     /* number of hard links */
    uid_t     st_uid;       /* user ID of owner */
    gid_t     st_gid;       /* group ID of owner */
    dev_t     st_rdev;      /* device ID (if special file) */
    off_t     st_size;      /* total size, in bytes */
    blksize_t st_blksize;   /* blocksize for file system I/O */
    blkcnt_t  st_blocks;    /* number of 512B blocks allocated */
    time_t    st_atime;     /* time of last access */
    time_t    st_mtime;     /* time of last modification */
    time_t    st_ctime;     /* time of last status change */
};

typedef uint64_t fsblkcnt_t;
typedef uint64_t fsfilcnt_t;

struct fs_statvfs_t {
    uint64_t   f_bsize;     /* file system block size */
    uint64_t   f_frsize;    /* fragment size */
    fsblkcnt_t f_blocks;    /* size of fs in f_frsize units */
    fsblkcnt_t f_bfree;     /* # free blocks */
    fsblkcnt_t f_bavail;    /* # free blocks for unprivileged users */
    fsfilcnt_t f_files;     /* # inodes */
    fsfilcnt_t f_ffree;     /* # free inodes */
    fsfilcnt_t f_favail;    /* # free inodes for unprivileged users */
    uint64_t   f_fsid;      /* file system ID */
    uint64_t   f_flag;      /* mount flags */
    uint64_t   f_namemax;   /* maximum filename length */
};

struct fs_base_t;

struct fs_factory_t {
    explicit fs_factory_t(char const *factory_name);
    virtual ~fs_factory_t();

    virtual fs_base_t *mount(fs_init_info_t *conn) = 0;
    static void register_factory(void *p);

    char const * const name;
};

struct fs_base_t {
    virtual ~fs_base_t() = 0;

    //
    // Startup and shutdown

    virtual void unmount() = 0;

    //
    // Identification of boot drive

    virtual bool is_boot() const = 0;

    //
    // Scan directories

    virtual int opendir(fs_file_info_t **fi, fs_cpath_t path) = 0;
    virtual ssize_t readdir(fs_file_info_t *fi, dirent_t* buf,
                            off_t offset) = 0;
    virtual int releasedir(fs_file_info_t *fi) = 0;

    //
    // Read directory entry information

    virtual int getattr(fs_cpath_t path, fs_stat_t* stbuf) = 0;
    virtual int access(fs_cpath_t path, int mask) = 0;
    virtual int readlink(fs_cpath_t path, char* buf, size_t size) = 0;

    //
    // Modify directories

    virtual int mknod(fs_cpath_t path, fs_mode_t mode, fs_dev_t rdev) = 0;
    virtual int mkdir(fs_cpath_t path, fs_mode_t mode) = 0;
    virtual int rmdir(fs_cpath_t path) = 0;
    virtual int symlink(fs_cpath_t to, fs_cpath_t from) = 0;
    virtual int rename(fs_cpath_t from, fs_cpath_t to) = 0;
    virtual int link(fs_cpath_t from, fs_cpath_t to) = 0;
    virtual int unlink(fs_cpath_t path) = 0;

    //
    // Modify directory entries

    virtual int fchmod(fs_file_info_t *fi,
                 fs_mode_t mode) = 0;
    virtual int fchown(fs_file_info_t *fi,
                 fs_uid_t uid,
                 fs_gid_t gid) = 0;
    virtual int truncate(fs_cpath_t path,
                    off_t size) = 0;
    virtual int utimens(fs_cpath_t path,
                   fs_timespec_t const *ts) = 0;

    //
    // Open/close files

    virtual int open(fs_file_info_t **fi,
                fs_cpath_t path, int flags, mode_t mode) = 0;
    virtual int release(fs_file_info_t *fi) = 0;

    //
    // Read/write files

    virtual ssize_t read(fs_file_info_t *fi,
                    char *buf,
                    size_t size,
                    off_t offset) = 0;
    virtual ssize_t write(fs_file_info_t *fi,
                     char const *buf,
                     size_t size,
                     off_t offset) = 0;
    virtual int ftruncate(fs_file_info_t *fi,
                     off_t offset) = 0;

    //
    // Query open file

    virtual int fstat(fs_file_info_t *fi,
                 fs_stat_t *st) = 0;

    //
    // Sync files and directories and flush buffers

    virtual int fsync(fs_file_info_t *fi,
                 int isdatasync) = 0;
    virtual int fsyncdir(fs_file_info_t *fi,
                    int isdatasync) = 0;
    virtual int flush(fs_file_info_t *fi) = 0;

    //
    // lock/unlock file

    virtual int lock(fs_file_info_t *fi,
                int cmd, fs_flock_t* locks) = 0;

    //
    // Get block map

    virtual int bmap(fs_cpath_t path, size_t blocksize,
                uint64_t* blockno) = 0;

    //
    // Get filesystem information

    virtual int statfs(fs_statvfs_t* stbuf) = 0;

    //
    // Read/Write/Enumerate extended attributes

    virtual int setxattr(fs_cpath_t path,
                    char const* name, char const* value,
                    size_t size, int flags) = 0;
    virtual int getxattr(fs_cpath_t path,
                    char const* name, char* value,
                    size_t size) = 0;
    virtual int listxattr(fs_cpath_t path,
                     char const* list, size_t size) = 0;

    //
    // ioctl API

    virtual int ioctl(fs_file_info_t *fi,
                 int cmd, void* arg,
                 unsigned int flags, void* data) = 0;

    //
    //

    virtual int poll(fs_file_info_t *fi,
                fs_pollhandle_t* ph, unsigned* reventsp) = 0;
};

#define FS_BASE_WR_IMPL \
    int mknod(fs_cpath_t path, fs_mode_t mode,                          \
              fs_dev_t rdev) override final;                            \
    int mkdir(fs_cpath_t path, fs_mode_t mode) override final;          \
    int rmdir(fs_cpath_t path) override final;                          \
    int symlink(fs_cpath_t to, fs_cpath_t from) override final;         \
    int rename(fs_cpath_t from, fs_cpath_t to) override final;          \
    int link(fs_cpath_t from, fs_cpath_t to) override final;            \
    int unlink(fs_cpath_t path) override final;                         \
    int fchmod(fs_file_info_t *fi,                                      \
         fs_mode_t mode) override final;                                \
    int fchown(fs_file_info_t *fi,                                      \
         fs_uid_t uid,                                                  \
         fs_gid_t gid) override final;                                  \
    int truncate(fs_cpath_t path,                                       \
            off_t size) override final;                                 \
    ssize_t write(fs_file_info_t *fi,                                   \
             char const *buf,                                           \
             size_t size,                                               \
             off_t offset) override final;                              \
    int ftruncate(fs_file_info_t *fi,                                   \
             off_t offset) override final;                              \
    int utimens(fs_cpath_t path,                                        \
           fs_timespec_t const *ts) override final;                     \
    int setxattr(fs_cpath_t path,                                       \
            char const* name, char const* value,                        \
            size_t size, int flags) override final;                     \
    int fsync(fs_file_info_t *fi,                                       \
         int isdatasync) override final;                                \
    int fsyncdir(fs_file_info_t *fi,                                    \
            int isdatasync) override final;                             \
    int flush(fs_file_info_t *fi) override final;                       \


#define FS_BASE_IMPL \
    void unmount() override final;                                      \
    bool is_boot() const override final;                                \
    int opendir(fs_file_info_t **fi, fs_cpath_t path) override final;   \
    ssize_t readdir(fs_file_info_t *fi, dirent_t* buf,                  \
                    off_t offset) override final;                       \
    int releasedir(fs_file_info_t *fi) override final;                  \
    int getattr(fs_cpath_t path, fs_stat_t* stbuf) override final;      \
    int access(fs_cpath_t path, int mask) override final;               \
    int readlink(fs_cpath_t path, char* buf,                            \
                 size_t size) override final;                           \
    int open(fs_file_info_t **fi,                                       \
        fs_cpath_t path, int flags, mode_t mode) override final;        \
    int release(fs_file_info_t *fi) override final;                     \
    ssize_t read(fs_file_info_t *fi,                                    \
            char *buf,                                                  \
            size_t size,                                                \
            off_t offset) override final;                               \
    int fstat(fs_file_info_t *fi,                                       \
         fs_stat_t *st) override final;                                 \
    int lock(fs_file_info_t *fi,                                        \
        int cmd, fs_flock_t* locks) override final;                     \
    int bmap(fs_cpath_t path, size_t blocksize,                         \
        uint64_t* blockno) override final;                              \
    int statfs(fs_statvfs_t* stbuf) override final;                     \
    int getxattr(fs_cpath_t path,                                       \
            char const* name, char* value,                              \
            size_t size) override final;                                \
    int listxattr(fs_cpath_t path,                                      \
             char const* list, size_t size) override final;             \
    int ioctl(fs_file_info_t *fi,                                       \
         int cmd, void* arg,                                            \
         unsigned int flags, void* data) override final;                \
    int poll(fs_file_info_t *fi,                                        \
        fs_pollhandle_t* ph, unsigned* reventsp) override final;

#define FS_BASE_RW_IMPL \
    FS_BASE_IMPL \
    FS_BASE_WR_IMPL

// Base for read-only filesystems,
// provides implementation for all methods that write
class EXPORT fs_base_ro_t : public fs_base_t {
    FS_BASE_WR_IMPL
};

#define FS_DEV_PTR(type, p) type *self = (type*)(p)

void fs_register_factory(fs_factory_t *fs);

//
// Partitioning scheme (MBR, GPT, and special types like CPIO, etc)

struct part_dev_t {
    storage_dev_base_t *drive;
    uint64_t lba_st;
    uint64_t lba_len;
    char const *name;
};

struct part_factory_t {
    explicit constexpr part_factory_t(char const * factory_name)
        : name(factory_name)
    {
    }

    virtual ~part_factory_t();

    virtual std::vector<part_dev_t*> detect(storage_dev_base_t *drive) = 0;
    static void register_factory(void *p);
    char const * const name;
};

__BEGIN_DECLS

void part_register_factory(part_factory_t *factory);

void fs_mount(char const *fs_name, fs_init_info_t *info);
void fs_add(fs_factory_t *reg, fs_base_t *fs);
fs_base_t *fs_from_id(size_t id);

void probe_storage_factory(storage_if_factory_t *factory);

__END_DECLS
