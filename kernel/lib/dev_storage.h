#pragma once
#include "types.h"

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

typedef struct STORAGE_DEV_T STORAGE_DEV_T;
typedef struct storage_dev_vtbl_t storage_dev_vtbl_t;

typedef struct STORAGE_IF_T STORAGE_IF_T;
typedef struct storage_if_vtbl_t storage_if_vtbl_t;

typedef struct storage_dev_base_t storage_dev_base_t;
typedef struct storage_if_base_t storage_if_base_t;

typedef struct if_list_t {
    void *base;
    unsigned stride;
    unsigned count;
} if_list_t;

//
// Storage Device (hard drive, CDROM, etc)

struct storage_dev_base_t {
    storage_dev_vtbl_t *vtbl;
    storage_if_base_t *if_;
};

typedef enum storage_dev_info_t {
    STORAGE_INFO_NONE = 0,
    STORAGE_INFO_BLOCKSIZE = 1,
    STORAGE_INFO_MAX = 0x7FFFFFFF
} storage_dev_info_t;

struct storage_dev_vtbl_t {
    // Startup/shutdown
    void (*cleanup)(storage_dev_base_t *);

    int (*read_blocks)(storage_dev_base_t *dev,
                void *data, uint64_t count,
                uint64_t lba);

    int (*write_blocks)(
            storage_dev_base_t *dev,
            void const *data, uint64_t count,
            uint64_t lba);

    int (*flush)(storage_dev_base_t *dev);

    long (*info)(storage_dev_base_t *dev,
                 storage_dev_info_t key);
};

//
// Storage Interface (IDE, AHCI, etc)

struct storage_if_base_t {
    storage_if_vtbl_t *vtbl;
};

struct storage_if_vtbl_t {
    if_list_t (*detect)(void);

    void (*cleanup)(storage_if_base_t *if_);

    if_list_t (*detect_devices)(storage_if_base_t *if_);
};

void register_storage_if_device(char const *name, storage_if_vtbl_t *vtbl);

#define MAKE_storage_if_VTBL(type, name) { \
    name##_if_detect, \
    name##_if_cleanup, \
    name##_if_detect_devices \
}

#define MAKE_storage_dev_VTBL(type, name) { \
    name##_dev_cleanup, \
    name##_dev_read_blocks, \
    name##_dev_write_blocks, \
    name##_dev_flush, \
    name##_dev_info \
}

#ifdef STORAGE_DEV_NAME
#define DECLARE_storage_if_DEVICE(name) \
    DECLARE_DEVICE(storage_if, name)

#define REGISTER_storage_if_DEVICE(name) \
    REGISTER_DEVICE(storage_if, name, 'L')

#define STORAGE_IF_DEV_PTR(dev) STORAGE_IF_T *self = (void*)dev

#define STORAGE_IF_DEV_PTR_UNUSED(dev) (void)dev

//

#define DECLARE_storage_dev_DEVICE(name) \
    DECLARE_DEVICE(storage_dev, name)

#define REGISTER_storage_dev_DEVICE(name) \
    REGISTER_DEVICE(storage_dev, name, 'L')

#define DEFINE_storage_dev_DEVICE(name) \
    DEFINE_DEVICE(storage_dev, name)

#define STORAGE_DEV_DEV_PTR(dev) STORAGE_DEV_T *self = (void*)dev

#define STORAGE_DEV_DEV_PTR_UNUSED(dev) (void)dev

#endif

typedef int dev_t;

storage_dev_base_t *open_storage_dev(dev_t dev);
void close_storage_dev(storage_dev_base_t *dev);

//
// Filesystem (FAT32, etc)

typedef struct fs_vtbl_t fs_vtbl_t;

typedef struct fs_base_t fs_base_t;

typedef char const *fs_cpath_t;

typedef uint16_t fs_mode_t;
typedef uint32_t fs_uid_t;
typedef uint32_t fs_gid_t;

typedef struct fs_init_info_t fs_init_info_t;

typedef struct fs_stat_t fs_stat_t;
typedef void fs_file_info_t;

typedef struct fs_statvfs_t fs_statvfs_t;

typedef struct fs_flock_t fs_flock_t;

typedef uint64_t fs_timespec_t;

typedef uint64_t fs_dev_t;

typedef struct fs_pollhandle_t fs_pollhandle_t;

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

struct fs_vtbl_t {
    //
    // Startup and shutdown

    // Returns extended fs_base_t
    void *(*mount)(fs_init_info_t *conn);
    void (*unmount)(fs_base_t *dev);

    //
    // Read directory entry information

    int (*getattr)(fs_base_t *dev,
                   fs_cpath_t path, fs_stat_t* stbuf);
    int (*access)(fs_base_t *dev,
                  fs_cpath_t path, int mask);
    int (*readlink)(fs_base_t *dev,
                    fs_cpath_t path, char* buf, size_t size);

    //
    // Scan directories

    int (*opendir)(fs_base_t *dev,
                   fs_cpath_t path, fs_file_info_t **fi);
    ssize_t (*readdir)(fs_base_t *dev,
                       fs_cpath_t path, void* buf, off_t offset,
                       fs_file_info_t *fi);
    int (*releasedir)(fs_base_t *dev,
                      fs_cpath_t path, fs_file_info_t *fi);

    //
    // Modify directories

    int (*mknod)(fs_base_t *dev,
                 fs_cpath_t path, fs_mode_t mode, fs_dev_t rdev);
    int (*mkdir)(fs_base_t *dev,
                 fs_cpath_t path, fs_mode_t mode);
    int (*rmdir)(fs_base_t *dev,
                 fs_cpath_t path);
    int (*symlink)(fs_base_t *dev,
                   fs_cpath_t to, fs_cpath_t from);
    int (*rename)(fs_base_t *dev,
                  fs_cpath_t from, fs_cpath_t to);
    int (*link)(fs_base_t *dev,
                fs_cpath_t from, fs_cpath_t to);
    int (*unlink)(fs_base_t *dev,
                  fs_cpath_t path);

    //
    // Modify directory entries

    int (*chmod)(fs_base_t *dev,
                 fs_cpath_t path, fs_mode_t mode);
    int (*chown)(fs_base_t *dev,
                 fs_cpath_t path, fs_uid_t uid, fs_gid_t gid);
    int (*truncate)(fs_base_t *dev,
                    fs_cpath_t path, off_t size);
    int (*utimens)(fs_base_t *dev,
                   fs_cpath_t path, const fs_timespec_t *ts);

    //
    // Open/close files

    int (*open)(fs_base_t *dev,
                fs_cpath_t path, fs_file_info_t **fi);
    int (*release)(fs_base_t *dev,
                   fs_cpath_t path, fs_file_info_t *fi);

    //
    // Read/write files

    ssize_t (*read)(fs_base_t *dev,
                fs_cpath_t path, char *buf,
                size_t size, off_t offset,
                fs_file_info_t *fi);
    ssize_t (*write)(fs_base_t *dev,
                 fs_cpath_t path, char *buf,
                 size_t size, off_t offset,
                 fs_file_info_t *fi);

    //
    // Sync files and directories and flush buffers

    int (*fsync)(fs_base_t *dev,
                 fs_cpath_t path, int isdatasync,
                 fs_file_info_t *fi);
    int (*fsyncdir)(fs_base_t *dev,
                    fs_cpath_t path, int isdatasync,
                    fs_file_info_t *fi);
    int (*flush)(fs_base_t *dev,
                 fs_cpath_t path, fs_file_info_t *fi);

    //
    // Get filesystem information

    int (*statfs)(fs_base_t *dev,
                  fs_cpath_t path, fs_statvfs_t* stbuf);

    //
    // lock/unlock file

    int (*lock)(fs_base_t *dev,
                fs_cpath_t path, fs_file_info_t *fi,
                int cmd, fs_flock_t* locks);

    //
    // Get block map

    int (*bmap)(fs_base_t *dev,
                fs_cpath_t path, size_t blocksize,
                uint64_t* blockno);

    //
    // Read/Write/Enumerate extended attributes

    int (*setxattr)(fs_base_t *dev,
                    fs_cpath_t path,
                    char const* name, char const* value,
                    size_t size, int flags);
    int (*getxattr)(fs_base_t *dev,
                    fs_cpath_t path,
                    char const* name, char* value,
                    size_t size);
    int (*listxattr)(fs_base_t *dev,
                     fs_cpath_t path,
                     char const* list, size_t size);

    //
    // ioctl API

    int (*ioctl)(fs_base_t *dev,
                 fs_cpath_t path, int cmd, void* arg,
                 fs_file_info_t *fi,
                 unsigned int flags, void* data);

    //
    //

    int (*poll)(fs_base_t *dev,
                fs_cpath_t path,
                fs_file_info_t *fi,
                fs_pollhandle_t* ph, unsigned* reventsp);
};

struct fs_base_t {
    fs_vtbl_t *vtbl;
};

#define MAKE_fs_VTBL(type, name) { \
    name##_mount,       \
    name##_unmount,     \
                        \
    name##_getattr,     \
    name##_access,      \
    name##_readlink,    \
                        \
    name##_opendir,     \
    name##_readdir,     \
    name##_releasedir,  \
                        \
    name##_mknod,       \
    name##_mkdir,       \
    name##_rmdir,       \
    name##_symlink,     \
    name##_rename,      \
    name##_link,        \
    name##_unlink,      \
                        \
    name##_chmod,       \
    name##_chown,       \
    name##_truncate,    \
    name##_utimens,     \
                        \
    name##_open,        \
    name##_release,     \
    name##_read,        \
    name##_write,       \
                        \
    name##_fsync,       \
    name##_fsyncdir,    \
    name##_flush,       \
                        \
    name##_statfs,      \
                        \
    name##_lock,        \
    name##_bmap,        \
                        \
    name##_setxattr,    \
    name##_getxattr,    \
    name##_listxattr,   \
                        \
    name##_ioctl,       \
    name##_poll         \
}

#ifdef FS_NAME
#define FS_T_EXPAND2(v) v ## _fs_t
#define FS_T_EXPAND(v) FS_T_EXPAND2(v)
#define FS_T FS_T_EXPAND(FS_NAME)

typedef struct FS_T FS_T;

#define DECLARE_fs_DEVICE(name) \
    DECLARE_DEVICE(fs, name)

#define REGISTER_fs_DEVICE(name) \
    REGISTER_DEVICE(fs, name, 'F')

#define DEFINE_fs_DEVICE(name) \
    DEFINE_DEVICE(fs, name)

#define FS_DEV_PTR(dev) FS_T *self = (void*)dev

#define FS_DEV_PTR_UNUSED(dev) (void)dev
#endif

void register_fs_device(char const *name, fs_vtbl_t *vtbl);

//
// Partitioning scheme (MBR, UEFI, etc)

typedef struct part_vtbl_t part_vtbl_t;

struct part_vtbl_t {
    if_list_t (*detect)(storage_dev_base_t *drive);
};

typedef struct part_dev_t part_dev_t;

struct part_dev_t {
    part_vtbl_t *vtbl;
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

void mount_fs(char const *fs_name, fs_init_info_t *info);
