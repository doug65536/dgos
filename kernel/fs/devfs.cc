#include "devfs.h"
#include <cxxstring.h>

struct dev_fs_file_info_t final
        : public fs_file_info_t
{
    // fs_file_info_t interface
public:
    ino_t get_inode() const override
    {
        return -int(errno_t::ENOSYS);
    }
};

static dev_fs_t dev_fs;

char const *dev_fs_t::name() const noexcept
{
    return "tmpfs";
}

void dev_fs_t::unmount()
{

}

bool dev_fs_t::is_boot() const
{
    return false;
}

int dev_fs_t::resolve(fs_file_info_t *dirfi, fs_cpath_t path,
                      size_t &consumed)
{
    return -1;
}

int dev_fs_t::opendirat(fs_file_info_t **fi,
                        fs_file_info_t *dirfi, fs_cpath_t path)
{
    return -int(errno_t::ENOENT);
}

ssize_t dev_fs_t::readdir(fs_file_info_t *fi, dirent_t* buf, off_t offset)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::releasedir(fs_file_info_t *fi)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::getattrat(fs_file_info_t *dirfi, fs_cpath_t path,
                        fs_stat_t* stbuf)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::accessat(fs_file_info_t *dirfi, fs_cpath_t path,
                       int mask)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::readlinkat(fs_file_info_t *dirfi, fs_cpath_t path,
                         char* buf, size_t size)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::mknodat(fs_file_info_t *dirfi, fs_cpath_t path,
                      fs_mode_t mode, fs_dev_t rdev)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::mkdirat(fs_file_info_t *dirfi, fs_cpath_t path,
                      fs_mode_t mode)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::rmdirat(fs_file_info_t *dirfi, fs_cpath_t path)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::symlinkat(fs_file_info_t *dirtofi, fs_cpath_t to,
                        fs_file_info_t *dirfromfi, fs_cpath_t from)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::renameat(fs_file_info_t *dirfromfi, fs_cpath_t from,
                       fs_file_info_t *dirtofi, fs_cpath_t to)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::linkat(fs_file_info_t *dirfromfi, fs_cpath_t from,
                     fs_file_info_t *dirtofi, fs_cpath_t to)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::unlinkat(fs_file_info_t *dirfi,
                       fs_cpath_t path)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::fchmod(fs_file_info_t *fi,
                     fs_mode_t mode)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::fchown(fs_file_info_t *fi,
                     fs_uid_t uid, fs_gid_t gid)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::truncateat(fs_file_info_t *dirfi, fs_cpath_t path,
                         off_t size)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::utimensat(fs_file_info_t *dirfi, fs_cpath_t path,
                        fs_timespec_t const *ts)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::openat(fs_file_info_t **fi,
                     fs_file_info_t *dirfi, fs_cpath_t path,
                     int flags, mode_t mode)
{
    // Lookup the file info factory for this device name

    size_t name_hash = dev_fs_file_reg_t::hash(path);

    for (dev_fs_file_reg_t *reg : files) {
        if (reg->name_hash == name_hash && reg->name == path) {
            *fi = reg->open(flags, mode);
            return 0;
        }
    }

    return -int(errno_t::ENOSYS);
}

int dev_fs_t::release(fs_file_info_t *fi)
{
    return -int(errno_t::ENOSYS);
}

ssize_t dev_fs_t::read(fs_file_info_t *fi,
        char *buf,
        size_t size,
        off_t offset)
{
    dev_fs_file_t *file = static_cast<dev_fs_file_t*>(fi);
    return file->read(buf, size, offset);
}

ssize_t dev_fs_t::write(fs_file_info_t *fi, char const *buf,
                        size_t size, off_t offset)
{
    dev_fs_file_t *file = static_cast<dev_fs_file_t*>(fi);
    return file->write(buf, size, offset);
}

int dev_fs_t::ftruncate(fs_file_info_t *fi, off_t offset)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::fstat(fs_file_info_t *fi, fs_stat_t *st)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::fsync(fs_file_info_t *fi, int isdatasync)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::fsyncdir(fs_file_info_t *fi, int isdatasync)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::flush(fs_file_info_t *fi)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::lock(fs_file_info_t *fi, int cmd, fs_flock_t* locks)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::bmapat(fs_file_info_t *dirfi, fs_cpath_t path,
                     size_t blocksize, uint64_t* blockno)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::statfs(fs_statvfs_t* stbuf)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::setxattrat(fs_file_info_t *dirfi, fs_cpath_t path,
                         char const* name, char const* value,
                         size_t size, int flags)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::getxattrat(fs_file_info_t *dirfi, fs_cpath_t path,
                         char const* name, char* value, size_t size)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::listxattrat(fs_file_info_t *dirfi, fs_cpath_t path,
                          char const* list, size_t size)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::ioctl(fs_file_info_t *fi, int cmd, void* arg,
                    unsigned int flags, void* data)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::poll(fs_file_info_t *fi, fs_pollhandle_t* ph, unsigned* reventsp)
{
    return -int(errno_t::ENOSYS);
}

static dev_fs_t *devfs_instance;

dev_fs_t *devfs_create()
{
    devfs_instance = new (ext::nothrow) dev_fs_t();
    callout_call(callout_type_t::devfs_ready);
    return devfs_instance;
}

void devfs_delete(dev_fs_t *dev_fs)
{
    delete dev_fs;
}

bool devfs_register(dev_fs_file_reg_t *reg)
{
    return devfs_instance->register_file(reg);
}

dev_fs_file_reg_t::~dev_fs_file_reg_t()
{
}

dev_fs_t::device_factory_t::~device_factory_t()
{
}
