#pragma once
#include "types.h"
#include "dirent.h"
#include "errno.h"
#include "cpu/atomic.h"
#include "mutex.h"

// Storage device interface (IDE, AHCI, etc)

#include "dev_registration.h"

//
// Forward declarations

struct if_list_t {
    void *base;
    unsigned stride;
    unsigned count;
};

//
// Storage Device (hard drive, CDROM, etc)

enum storage_dev_info_t : uint32_t {
    STORAGE_INFO_NONE = 0,
    STORAGE_INFO_BLOCKSIZE,
    STORAGE_INFO_HAVE_TRIM
};

// I/O completion
struct iocp_t {
    typedef void (*callback_t)(errno_t err, uintptr_t arg);

    iocp_t(callback_t callback, uintptr_t arg)
        : done_count(0)
        , expect_count(0)
        , callback(callback)
        , arg(arg)
        , err(errno_t::OK)
    {
    }

    // A single request can be split into multiple requests at
    // the device driver layer. This allows a single completion
    // to be shared by all of the split operations. The actual
    // callback will be invoked when invoke is called `expect`
    // times. If this is not called before invoke, invoke will
    // immediately invoke the actual callback.
    void set_expect(uint16_t expect)
    {
        unique_lock<spinlock> hold(lock);
        expect_count = expect;

        if (done_count == expect)
            invoke_once();
    }

    void set_result(errno_t err_code)
    {
        err = err_code;
    }

    void invoke()
    {
        unique_lock<spinlock> hold(lock);
        if (++done_count >= expect_count)
            invoke_once();
    }

    void operator()()
    {
        invoke();
    }

    errno_t get_error() const
    {
        return err;
    }

private:
    void invoke_once()
    {
        if (callback != nullptr) {
            callback(err, arg);
            callback = nullptr;
        }
    }

    uint16_t volatile done_count;
    uint16_t expect_count;
    spinlock lock;
    callback_t callback;
    uintptr_t arg;
    errno_t err;
};

class blocking_iocp_t {
public:
    blocking_iocp_t()
        : iocp(&blocking_iocp_t::handler, uintptr_t(this))
        , done(false)
    {
    }

    static void handler(errno_t err, uintptr_t arg)
    {
        return ((blocking_iocp_t*)arg)->handler(err);
    }

    void handler(errno_t)
    {
        unique_lock<spinlock> hold(lock);
        assert(!done);
        done = true;
        hold.unlock();
        done_cond.notify_all();
    }

    operator iocp_t*()
    {
        return &iocp;
    }

    errno_t wait()
    {
        unique_lock<spinlock> hold(lock);
        while (!done)
            done_cond.wait(hold);
        return iocp.get_error();
    }

private:
    iocp_t iocp;
    spinlock lock;
    condition_variable done_cond;
    bool done;
};

struct storage_dev_base_t {
    // Startup/shutdown
    virtual void cleanup() = 0;

    //
    // Asynchronous I/O

    typedef void (*completion_callback_t)(errno_t err, uintptr_t arg);

    virtual errno_t read_async(
            void *data, int64_t count, uint64_t lba,
            iocp_t *iocp) = 0;

    virtual errno_t write_async(
            void const *data, int64_t count, uint64_t lba, bool fua,
            iocp_t *iocp) = 0;

    virtual errno_t trim_async(
            int64_t count, uint64_t lba,
            iocp_t *iocp) = 0;

    virtual errno_t flush_async(
            iocp_t *iocp) = 0;

    // Synchronous wrappers

    virtual int64_t read_blocks(
            void *data, int64_t count, uint64_t lba);

    virtual int64_t write_blocks(
            void const *data, int64_t count, uint64_t lba, bool fua);

    virtual int64_t trim_blocks(int64_t count, uint64_t lba);

    virtual int flush();

    virtual long info(storage_dev_info_t key) = 0;
};

#define STORAGE_DEV_IMPL                        \
    void cleanup() final;                       \
                                                \
    errno_t read_async(                         \
            void *data, int64_t count,          \
            uint64_t lba,                       \
            iocp_t *iocp) final;                \
                                                \
    errno_t write_async(                        \
            void const *data, int64_t count,    \
            uint64_t lba, bool fua,             \
            iocp_t *iocp) final;                \
                                                \
    errno_t flush_async(                        \
            iocp_t *iocp) final;                \
                                                \
    errno_t trim_async(                         \
            int64_t count,                      \
            uint64_t lba,                       \
            iocp_t *iocp) final;                \
                                                \
    long info(storage_dev_info_t key) final;

//
// Storage Interface (IDE, AHCI, etc)

struct storage_if_factory_t {
    storage_if_factory_t(char const *factory_name);
    virtual if_list_t detect(void) = 0;
    static void register_factory(void *p);
    char const * const name;
};

struct storage_if_base_t {
    virtual void cleanup() = 0;
    virtual if_list_t detect_devices() = 0;
};

#define STORAGE_IF_IMPL                 \
    void cleanup() final;               \
    if_list_t detect_devices() final;

#define STORAGE_REGISTER_FACTORY(name) \
    REGISTER_CALLOUT(& name##_factory_t::register_factory, \
        & name##_factory, callout_type_t::late_dev, "000")

void storage_if_register_factory(char const *name,
                                storage_if_factory_t *factory);

typedef int dev_t;

storage_dev_base_t *open_storage_dev(dev_t dev);
void close_storage_dev(storage_dev_base_t *dev);

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
    virtual fs_base_t *mount(fs_init_info_t *conn) = 0;
    static void register_factory(void *p);

    char const * const name;
};

struct fs_base_t {
    //
    // Startup and shutdown

    virtual void unmount() = 0;

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

    virtual int chmod(fs_cpath_t path,
                 fs_mode_t mode) = 0;
    virtual int chown(fs_cpath_t path,
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

#define FS_BASE_IMPL \
    void unmount() final;                                               \
    int opendir(fs_file_info_t **fi, fs_cpath_t path) final;            \
    ssize_t readdir(fs_file_info_t *fi, dirent_t* buf,                  \
                    off_t offset) final;                                \
    int releasedir(fs_file_info_t *fi) final;                           \
    int getattr(fs_cpath_t path, fs_stat_t* stbuf) final;               \
    int access(fs_cpath_t path, int mask) final;                        \
    int readlink(fs_cpath_t path, char* buf, size_t size) final;        \
    int mknod(fs_cpath_t path, fs_mode_t mode, fs_dev_t rdev) final;    \
    int mkdir(fs_cpath_t path, fs_mode_t mode) final;                   \
    int rmdir(fs_cpath_t path) final;                                   \
    int symlink(fs_cpath_t to, fs_cpath_t from) final;                  \
    int rename(fs_cpath_t from, fs_cpath_t to) final;                   \
    int link(fs_cpath_t from, fs_cpath_t to) final;                     \
    int unlink(fs_cpath_t path) final;                                  \
    int chmod(fs_cpath_t path,                                          \
         fs_mode_t mode) final;                                         \
    int chown(fs_cpath_t path,                                          \
         fs_uid_t uid,                                                  \
         fs_gid_t gid) final;                                           \
    int truncate(fs_cpath_t path,                                       \
            off_t size) final;                                          \
    int utimens(fs_cpath_t path,                                        \
           fs_timespec_t const *ts) final;                              \
    int open(fs_file_info_t **fi,                                       \
        fs_cpath_t path, int flags, mode_t mode) final;                 \
    int release(fs_file_info_t *fi) final;                              \
    ssize_t read(fs_file_info_t *fi,                                    \
            char *buf,                                                  \
            size_t size,                                                \
            off_t offset) final;                                        \
    ssize_t write(fs_file_info_t *fi,                                   \
             char const *buf,                                           \
             size_t size,                                               \
             off_t offset) final;                                       \
    int ftruncate(fs_file_info_t *fi,                                   \
             off_t offset) final;                                       \
    int fstat(fs_file_info_t *fi,                                       \
         fs_stat_t *st) final;                                          \
    int fsync(fs_file_info_t *fi,                                       \
         int isdatasync) final;                                         \
    int fsyncdir(fs_file_info_t *fi,                                    \
            int isdatasync) final;                                      \
    int flush(fs_file_info_t *fi) final;                                \
    int lock(fs_file_info_t *fi,                                        \
        int cmd, fs_flock_t* locks) final;                              \
    int bmap(fs_cpath_t path, size_t blocksize,                         \
        uint64_t* blockno) final;                                       \
    int statfs(fs_statvfs_t* stbuf) final;                              \
    int setxattr(fs_cpath_t path,                                       \
            char const* name, char const* value,                        \
            size_t size, int flags) final;                              \
    int getxattr(fs_cpath_t path,                                       \
            char const* name, char* value,                              \
            size_t size) final;                                         \
    int listxattr(fs_cpath_t path,                                      \
             char const* list, size_t size) final;                      \
    int ioctl(fs_file_info_t *fi,                                       \
         int cmd, void* arg,                                            \
         unsigned int flags, void* data) final;                         \
    int poll(fs_file_info_t *fi,                                        \
        fs_pollhandle_t* ph, unsigned* reventsp) final;

#define FS_DEV_PTR(type, p) type *self = (type*)(p)

void fs_register_factory(char const *name, fs_factory_t *fs);

//
// Partitioning scheme (MBR, UEFI, etc)

struct part_factory_t {
    explicit part_factory_t(char const * factory_name);
    virtual if_list_t detect(storage_dev_base_t *drive) = 0;
    static void register_factory(void *p);
    char const * const name;
};

struct part_dev_t;

struct part_dev_t {
    storage_dev_base_t *drive;
    uint64_t lba_st;
    uint64_t lba_len;
    char const *name;
};

void part_register_factory(char const *name, part_factory_t *factory);

void fs_mount(char const *fs_name, fs_init_info_t *info);
fs_base_t *fs_from_id(size_t id);
