#pragma once
#include "types.h"
#include "dirent.h"

// Storage device interface (IDE, AHCI, etc)

#include "dev_registration.h"

#define STORAGE_EXPAND_2(p,s) p ## s
#define STORAGE_EXPAND(p,s) STORAGE_EXPAND_2(p,s)

#define STORAGE_EXPAND1_3(t) t
#define STORAGE_EXPAND1_2(t) STORAGE_EXPAND1_3(t)
#define STORAGE_EXPAND1(t) STORAGE_EXPAND1_2(t)

#define STORAGE_IF_T STORAGE_EXPAND(STORAGE_DEV_NAME, _if_t)
#define STORAGE_DEV_T STORAGE_EXPAND(STORAGE_DEV_NAME, _dev_t)

//
// Forward declarations

struct STORAGE_DEV_T;
struct storage_dev_vtbl_t;

struct STORAGE_IF_T;
struct storage_if_vtbl_t;

struct storage_dev_base_t;
struct storage_if_base_t;

typedef struct if_list_t {
    void *base;
    unsigned stride;
    unsigned count;
} if_list_t;

//
// Storage Device (hard drive, CDROM, etc)

typedef enum storage_dev_info_t {
    STORAGE_INFO_NONE = 0,
    STORAGE_INFO_BLOCKSIZE = 1,
    STORAGE_INFO_MAX = 0x7FFFFFFF
} storage_dev_info_t;

struct storage_dev_base_t {
    // Startup/shutdown
    virtual void cleanup(storage_dev_base_t *) = 0;

    virtual int read_blocks(storage_dev_base_t *dev,
                void *data, uint64_t count,
                uint64_t lba) = 0;

    virtual int write_blocks(
            storage_dev_base_t *dev,
            void const *data, uint64_t count,
            uint64_t lba) = 0;

    virtual int flush(storage_dev_base_t *dev) = 0;

    virtual long info(storage_dev_base_t *dev,
                 storage_dev_info_t key) = 0;
};

//
// Storage Interface (IDE, AHCI, etc)

struct storage_if_base_t {
    virtual if_list_t detect(void) = 0;

    virtual void cleanup(storage_if_base_t *if_) = 0;

    virtual if_list_t detect_devices(storage_if_base_t *if_) = 0;
};

void register_storage_if_device(char const *name, storage_if_base_t *vtbl);

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
typedef void fs_file_info_t;

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
    virtual fs_base_t *mount(fs_init_info_t *conn) = 0;
};

struct fs_base_t {
    //
    // Startup and shutdown

    virtual void unmount() = 0;

    //
    // Scan directories

    virtual int opendir(fs_base_t *dev,
                   fs_file_info_t **fi,
                   fs_cpath_t path) = 0;
    virtual ssize_t readdir(fs_base_t *dev,
                       fs_file_info_t *fi,
                       dirent_t* buf,
                       off_t offset) = 0;
    virtual int releasedir(fs_base_t *dev,
                      fs_file_info_t *fi) = 0;

    //
    // Read directory entry information

    virtual int getattr(fs_base_t *dev,
                   fs_cpath_t path, fs_stat_t* stbuf) = 0;
    virtual int access(fs_base_t *dev,
                  fs_cpath_t path, int mask) = 0;
    virtual int readlink(fs_base_t *dev,
                    fs_cpath_t path, char* buf, size_t size) = 0;

    //
    // Modify directories

    virtual int mknod(fs_base_t *dev,
                 fs_cpath_t path, fs_mode_t mode, fs_dev_t rdev) = 0;
    virtual int mkdir(fs_base_t *dev,
                 fs_cpath_t path, fs_mode_t mode) = 0;
    virtual int rmdir(fs_base_t *dev,
                 fs_cpath_t path) = 0;
    virtual int symlink(fs_base_t *dev,
                   fs_cpath_t to, fs_cpath_t from) = 0;
    virtual int rename(fs_base_t *dev,
                  fs_cpath_t from, fs_cpath_t to) = 0;
    virtual int link(fs_base_t *dev,
                fs_cpath_t from, fs_cpath_t to) = 0;
    virtual int unlink(fs_base_t *dev,
                  fs_cpath_t path) = 0;

    //
    // Modify directory entries

    virtual int chmod(fs_base_t *dev,
                 fs_cpath_t path,
                 fs_mode_t mode) = 0;
    virtual int chown(fs_base_t *dev,
                 fs_cpath_t path,
                 fs_uid_t uid,
                 fs_gid_t gid) = 0;
    virtual int truncate(fs_base_t *dev,
                    fs_cpath_t path,
                    off_t size) = 0;
    virtual int utimens(fs_base_t *dev,
                   fs_cpath_t path,
                   fs_timespec_t const *ts) = 0;

    //
    // Open/close files

    virtual int open(fs_base_t *dev,
                fs_file_info_t **fi,
                fs_cpath_t path) = 0;
    virtual int release(fs_base_t *dev,
                   fs_file_info_t *fi) = 0;

    //
    // Read/write files

    virtual ssize_t read(fs_base_t *dev,
                    fs_file_info_t *fi,
                    char *buf,
                    size_t size,
                    off_t offset) = 0;
    virtual ssize_t write(fs_base_t *dev,
                     fs_file_info_t *fi,
                     char const *buf,
                     size_t size,
                     off_t offset) = 0;
    virtual int ftruncate(fs_base_t *dev,
                     fs_file_info_t *fi,
                     off_t offset) = 0;

    //
    // Query open file

    virtual int fstat(fs_base_t *dev,
                 fs_file_info_t *fi,
                 fs_stat_t *st) = 0;

    //
    // Sync files and directories and flush buffers

    virtual int fsync(fs_base_t *dev,
                 fs_file_info_t *fi,
                 int isdatasync) = 0;
    virtual int fsyncdir(fs_base_t *dev,
                    fs_file_info_t *fi,
                    int isdatasync) = 0;
    virtual int flush(fs_base_t *dev,
                 fs_file_info_t *fi) = 0;

    //
    // lock/unlock file

    virtual int lock(fs_base_t *dev,
                fs_file_info_t *fi,
                int cmd, fs_flock_t* locks) = 0;

    //
    // Get block map

    virtual int bmap(fs_base_t *dev,
                fs_cpath_t path, size_t blocksize,
                uint64_t* blockno) = 0;

    //
    // Get filesystem information

    virtual int statfs(fs_base_t *dev,
                  fs_statvfs_t* stbuf) = 0;

    //
    // Read/Write/Enumerate extended attributes

    virtual int setxattr(fs_base_t *dev,
                    fs_cpath_t path,
                    char const* name, char const* value,
                    size_t size, int flags) = 0;
    virtual int getxattr(fs_base_t *dev,
                    fs_cpath_t path,
                    char const* name, char* value,
                    size_t size) = 0;
    virtual int listxattr(fs_base_t *dev,
                     fs_cpath_t path,
                     char const* list, size_t size) = 0;

    //
    // ioctl API

    virtual int ioctl(fs_base_t *dev,
                 fs_file_info_t *fi,
                 int cmd, void* arg,
                 unsigned int flags, void* data) = 0;

    //
    //

    virtual int poll(fs_base_t *dev,
                fs_file_info_t *fi,
                fs_pollhandle_t* ph, unsigned* reventsp) = 0;
};

void register_fs_device(char const *name, fs_factory_t *fs);

//
// Partitioning scheme (MBR, UEFI, etc)

struct part_vtbl_t;

struct part_base_t {
    virtual if_list_t detect(storage_dev_base_t *drive) = 0;
};

struct part_dev_t;

struct part_dev_t : public part_base_t {
    storage_dev_base_t *drive;
    uint64_t lba_st;
    uint64_t lba_len;
    char const *name;
};

#define MAKE_part_VTBL(type, name) { \
    name##_##type##_detect \
}

#define DECLARE_part_DEVICE(name) \
    DECLARE_DEVICE(part, name)

#define REGISTER_part_DEVICE(name) \
    REGISTER_DEVICE(part, name, 'P')

#define DEFINE_part_DEVICE(name) \
    DEFINE_DEVICE(part, name)

#define PART_DEV_PTR(dev) PART_T *self

void register_part_device(char const *name, part_vtbl_t *vtbl);

void fs_mount(char const *fs_name, fs_init_info_t *info);
fs_base_t *fs_from_id(size_t id);
