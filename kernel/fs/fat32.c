#define FS_NAME fat_fs_t
#define STORAGE_IMPL
#include "dev_storage.h"

DECLARE_fs_DEVICE(fat32);

//
// Startup and shutdown

static void* fat32_mount(fs_init_info_t *conn)
{
    (void)conn;
    return 0;
}

static void fat32_unmount(fs_base_t *dev)
{
    FS_DEV_PTR_UNUSED(dev);
}

//
// Read directory entry information

static int fat32_getattr(fs_base_t *dev,
                         fs_cpath_t path, fs_stat_t* stbuf)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)stbuf;
    return 0;
}

static int fat32_access(fs_base_t *dev,
                        fs_cpath_t path, int mask)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mask;
    return 0;
}

static int fat32_readlink(fs_base_t *dev,
                          fs_cpath_t path, char* buf, size_t size)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)buf;
    (void)size;
    return 0;
}

//
// Scan directories

static int fat32_opendir(fs_base_t *dev,
                         fs_file_info_t **fi,
                         fs_cpath_t path)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)fi;
    return 0;
}

static ssize_t fat32_readdir(fs_base_t *dev,
                             fs_file_info_t *fi,
                             void* buf,
                             off_t offset)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)buf;
    (void)offset;
    (void)fi;
    return 0;
}

static int fat32_releasedir(fs_base_t *dev,
                            fs_file_info_t *fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
    return 0;
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
    return 0;
}

static int fat32_mkdir(fs_base_t *dev,
                       fs_cpath_t path, fs_mode_t mode)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mode;
    return 0;
}

static int fat32_rmdir(fs_base_t *dev,
                       fs_cpath_t path)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    return 0;
}

static int fat32_symlink(fs_base_t *dev,
                         fs_cpath_t to, fs_cpath_t from)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)to;
    (void)from;
    return 0;
}

static int fat32_rename(fs_base_t *dev,
                        fs_cpath_t from, fs_cpath_t to)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)from;
    (void)to;
    return 0;
}

static int fat32_link(fs_base_t *dev,
                      fs_cpath_t from, fs_cpath_t to)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)from;
    (void)to;
    return 0;
}

static int fat32_unlink(fs_base_t *dev,
                        fs_cpath_t path)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    return 0;
}

//
// Modify directory entries

static int fat32_chmod(fs_base_t *dev,
                       fs_cpath_t path, fs_mode_t mode)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)mode;
    return 0;
}

static int fat32_chown(fs_base_t *dev,
                       fs_cpath_t path, fs_uid_t uid, fs_gid_t gid)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)uid;
    (void)gid;
    return 0;
}

static int fat32_truncate(fs_base_t *dev,
                          fs_cpath_t path, off_t size)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)size;
    return 0;
}

static int fat32_utimens(fs_base_t *dev,
                         fs_cpath_t path, const fs_timespec_t *ts)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)ts;
    return 0;
}


//
// Open/close files

static int fat32_open(fs_base_t *dev,
                      fs_file_info_t **fi,
                      fs_cpath_t path)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)path;
    (void)fi;
    return 0;
}

static int fat32_release(fs_base_t *dev,
                         fs_file_info_t *fi)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)fi;
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
    FS_DEV_PTR_UNUSED(dev);
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    return 0;
}

static ssize_t fat32_write(fs_base_t *dev,
                           fs_file_info_t *fi,
                           char *buf,
                           size_t size,
                           off_t offset)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    return 0;
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
// Get filesystem information

static int fat32_statfs(fs_base_t *dev,
                        fs_statvfs_t* stbuf)
{
    FS_DEV_PTR_UNUSED(dev);
    (void)stbuf;
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
    return 0;
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
    return 0;
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
    return 0;
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
    return 0;
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
    return 0;
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
    return 0;
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
    return 0;
}

REGISTER_fs_DEVICE(fat32);
