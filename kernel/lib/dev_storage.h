#pragma once
#include "types.h"

// Storage device interface

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

//
// Storage Device

struct storage_dev_base_t {
    storage_dev_vtbl_t *vtbl;
    storage_if_base_t *if_;
};

typedef struct storage_dev_list_t {
    void *base;
    unsigned stride;
    unsigned count;
} storage_dev_list_t;

struct storage_dev_vtbl_t {
    // Startup/shutdown
    void (*cleanup)(storage_dev_base_t *);

    int (*read)(storage_dev_base_t *dev,
                void *data, uint64_t count,
                uint64_t lba);

    int (*write)(storage_dev_base_t *dev,
                 void *data, uint64_t count,
                 uint64_t lba);

    int (*flush)(storage_dev_base_t *dev);
};

//
// Storage Interface

struct storage_if_base_t {
    storage_if_vtbl_t *vtbl;
};

typedef struct if_list_t {
    void *base;
    unsigned stride;
    unsigned count;
} if_list_t;

struct storage_if_vtbl_t {
    if_list_t (*detect)(void);

    void (*cleanup)(storage_if_base_t *if_);

    storage_dev_list_t (*detect_devices)(storage_if_base_t *if_);
};

void register_storage_if_device(char const *name, storage_if_vtbl_t *vtbl);

#define MAKE_storage_if_VTBL(name) { \
    name##_detect, \
    name##_cleanup, \
    name##_detect_devices \
}

#define MAKE_storage_dev_VTBL(name) { \
    name##_cleanup, \
    name##_read, \
    name##_write, \
    name##_flush \
}

#ifdef STORAGE_IMPL
#define DECLARE_storage_if_DEVICE(name) \
    DECLARE_DEVICE(storage_if, name ## _if)

#define REGISTER_storage_if_DEVICE(name) \
    REGISTER_DEVICE(storage_if, name ## _if, 'L')

#define STORAGE_IF_DEV_PTR(dev) STORAGE_IF_T *self = (void*)dev

#define STORAGE_IF_DEV_PTR_UNUSED(dev) (void)dev

//

#define DECLARE_storage_dev_DEVICE(name) \
    DECLARE_DEVICE(storage_dev, name ## _dev)

#define REGISTER_storage_dev_DEVICE(name) \
    REGISTER_DEVICE(storage_dev, name ## _dev, 'L')

#define DEFINE_storage_dev_DEVICE(name) \
    DEFINE_DEVICE(storage_dev, name ## _dev)

#define STORAGE_DEV_DEV_PTR(dev) STORAGE_DEV_T *self = (void*)dev

#define STORAGE_DEV_DEV_PTR_UNUSED(dev) (void)dev

#endif

storage_dev_base_t *open_storage_dev(size_t dev);
void close_storage_dev(storage_dev_base_t *dev);

//
// Filesystem

typedef struct fs_vtbl_t fs_vtbl_t;

typedef char const *fs_cpath_t;

typedef uint16_t fs_mode_t;
typedef uint32_t fs_uid_t;
typedef uint32_t fs_gid_t;

typedef struct fs_init_info fs_init_info;

typedef struct fs_stat_t fs_stat_t;
typedef struct fs_file_info_t fs_file_info_t;

typedef struct fs_statvfs_t fs_statvfs_t;

typedef struct fs_flock_t fs_flock_t;

typedef uint64_t fs_timespec_t;

typedef uint64_t fs_dev_t;

typedef struct fs_pollhandle_t fs_pollhandle_t;

struct fs_vtbl_t {
    //
    // Startup and shutdown
    void* (*init)(fs_init_info *conn);
    void (*destroy)(void* private_data);

    //
    // Read directory entry information
    int (*getattr)(fs_cpath_t path, fs_stat_t* stbuf);
    int (*access)(fs_cpath_t path, int mask);
    int (*readlink)(fs_cpath_t path, char* buf, size_t size);

    //
    // Scan directories
    int (*opendir)(fs_cpath_t path, fs_file_info_t* fi);
    int (*readdir)(fs_cpath_t path, void* buf, off_t offset,
                   fs_file_info_t* fi);
    int (*releasedir)(fs_cpath_t path, fs_file_info_t *fi);

    //
    // Modify directories
    int (*mknod)(fs_cpath_t path, fs_mode_t mode, fs_dev_t rdev);
    int (*mkdir)(fs_cpath_t path, fs_mode_t mode);
    int (*rmdir)(fs_cpath_t path);
    int (*symlink)(fs_cpath_t to, fs_cpath_t from);
    int (*rename)(fs_cpath_t from, fs_cpath_t to);
    int (*link)(fs_cpath_t from, fs_cpath_t to);
    int (*unlink)(fs_cpath_t path);

    //
    // Modiy directory entries
    int (*chmod)(fs_cpath_t path, fs_mode_t mode);
    int (*chown)(fs_cpath_t path, fs_uid_t uid, fs_gid_t gid);
    int (*truncate)(fs_cpath_t path, off_t size);
    int (*utimens)(fs_cpath_t path, const fs_timespec_t *ts);

    //
    // Open/close files
    int (*open)(fs_cpath_t path, fs_file_info_t* fi);
    int (*release)(fs_cpath_t path, fs_file_info_t *fi);

    //
    // Read/write files
    int (*read)(fs_cpath_t path, char *buf,
                size_t size, off_t offset,
                fs_file_info_t* fi);
    int (*write)(fs_cpath_t path, char *buf,
                 size_t size, off_t offset,
                 fs_file_info_t* fi);

    //
    // Sync files and directories and flush buffers
    int (*fsync)(fs_cpath_t path, int isdatasync,
                 fs_file_info_t* fi);
    int (*fsyncdir)(fs_cpath_t path, int isdatasync,
                    fs_file_info_t* fi);
    int (*flush)(fs_cpath_t path, fs_file_info_t* fi);

    //
    // Get filesystem information
    int (*statfs)(fs_cpath_t path, fs_statvfs_t* stbuf);

    //
    // lock/unlock file
    int (*lock)(fs_cpath_t path, fs_file_info_t* fi,
                int cmd, fs_flock_t* locks);

    //
    // Get block map
    int (*bmap)(fs_cpath_t path, size_t blocksize,
                uint64_t* blockno);

    //
    // Read/Write/Enumerate extended attributes
    int (*setxattr)(fs_cpath_t path,
                    const char* name, const char* value,
                    size_t size, int flags);
    int (*getxattr)(fs_cpath_t path,
                    const char* name, char* value,
                    size_t size);
    int (*listxattr)(fs_cpath_t path,
                     const char* list, size_t size);

    //
    // ioctl API
    int (*ioctl)(fs_cpath_t path, int cmd, void* arg,
                 fs_file_info_t* fi,
                 unsigned int flags, void* data);

    //
    //
    int (*poll)(fs_cpath_t path,
                fs_file_info_t* fi,
                fs_pollhandle_t* ph, unsigned* reventsp);
};

#define MAKE_fs_VTBL(name) { \
    name##_init,        \
    name##_destroy,     \
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

#define DECLARE_fs_DEVICE(name) \
    DECLARE_DEVICE(fs, name ## _if)

#define REGISTER_fs_DEVICE(name) \
    REGISTER_DEVICE(fs, name ## _if, 'F')

#define FS_DEV_PTR(dev) STORAGE_IF_T *self = (void*)dev

#define FS_DEV_PTR_UNUSED(dev) (void)dev
