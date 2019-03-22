#include "tmpfs.h"
#include "dev_storage.h"

#include "likely.h"
#include "mm.h"
#include "rbtree.h"
#include "unique_ptr.h"
#include "algorithm.h"
#include "memory.h"
#include "cxxstring.h"
#include "vector.h"
#include "hash.h"

#define DEBUG_TMPFS 1
#if DEBUG_TMPFS
#define TMPFS_TRACE(...) printdbg("tmpfs: " __VA_ARGS__)
#else
#define TMPFS_TRACE(...) ((void)0)
#endif

static void *initrd_st;
static size_t initrd_sz;

class tmpfs_fs_t final : public fs_base_t {
public:
    void* mount(fs_init_info_t *conn);
    FS_BASE_RW_IMPL

private:
    friend class tmpfs_factory_t;

    std::vector<char> names;

    struct file_t {
        size_t name_ofs = 0;
        size_t name_sz = 0;
        size_t file_ofs = 0;
        uint32_t name_hash = 0;
        uint32_t file_sz = 0;
        uint32_t mtime = 0;
        uint16_t ino = 0;
        uint16_t mode = 0;
        uint16_t nlink = 0;
    };

    struct cpio_hdr_t {
        uint16_t magic;
        uint16_t dev;

        uint16_t ino;
        uint16_t mode;

        uint16_t uid;
        uint16_t gid;

        uint16_t nlink;
        uint16_t rdev;

        // Strangely stored as upper then lower halves in native endianness
        uint16_t mtime_parts[2];

        uint16_t namesize;

        // Strangely stored as upper then lower halves in native endianness
        uint16_t filesize_parts[2];

        uint32_t filesize() const
        {
            return (filesize_parts[0] << 16) | filesize_parts[1];
        }

        uint32_t mtime() const
        {
            return (mtime_parts[0] << 16) | mtime_parts[1];
        }

        char const *name() const
        {
            return (char const *)(this + 1);
        }

        void const *data() const
        {
            return (char const *)(this + 1) + namesize + (namesize & 1);
        }

        cpio_hdr_t const *next(char const *en) const
        {
            if (unlikely(namesize == 10 &&
                         !memcmp(name(), "TRAILER!!", 10) &&
                         filesize() == 0))
                return nullptr;

            cpio_hdr_t const *next_hdr = (cpio_hdr_t const *)
                    ((char const *)data() + (filesize() + (filesize() & 1)));

            // Catch overrun
            if (unlikely((char const *)(next_hdr + 1) > en))
                return nullptr;

            return next_hdr;
        }
    } _packed;

    cpio_hdr_t const *add(cpio_hdr_t const *hdr)
    {
        if (unlikely(hdr->magic != 0x71c7))
            return nullptr;

        file_t file{};
        file.file_ofs = (char*)hdr->data() - st;
        file.name_ofs = names.size();
        file.name_sz = hdr->namesize;
        file.file_sz = hdr->filesize();
        file.name_hash = likely(hdr->namesize)
                ? hash_32(hdr->name(), hdr->namesize - 1)
                : 0;
        file.ino = hdr->ino;
        file.mode = hdr->mode;
        file.mtime = hdr->mtime();
        file.nlink = hdr->nlink;

        char const *name = hdr->name();
        for (size_t i = 0, e = hdr->namesize; i < e; ++i) {
            if (unlikely(!names.push_back(name[i])))
                return nullptr;
        }

        if (unlikely(!files.push_back(file)))
            return nullptr;

        TMPFS_TRACE("added %s\n", hdr->name());
        return hdr->next(en);
    }

    std::vector<file_t> files;
    char const *st;
    char const *en;
};

//
// Startup and shutdown

void* tmpfs_fs_t::mount(fs_init_info_t *conn)
{
    // The mount parameters are unusual in this case
    // We receive the pointer and size as part_st and part_len

    st = (char const *)conn->part_st;
    en = (st + conn->part_len);

    names.resize(1);

    // Index the files
    cpio_hdr_t const *next_hdr = nullptr;
    for (cpio_hdr_t const *hdr = (cpio_hdr_t const *)st;
         hdr && ((char const *)(hdr + 1) < en);
         hdr = next_hdr) {
        if (unlikely(!add(hdr)))
            break;

        next_hdr = hdr->next(en);
    }

    names.shrink_to_fit();
    files.shrink_to_fit();

    return this;
}

void tmpfs_fs_t::unmount()
{
}

bool tmpfs_fs_t::is_boot() const
{
    return true;
}

//
// Read directory entry information

int tmpfs_fs_t::getattr(fs_cpath_t path, fs_stat_t* stbuf)
{
    (void)path;
    (void)stbuf;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::access(fs_cpath_t path, int mask)
{
    (void)path;
    (void)mask;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::readlink(fs_cpath_t path, char* buf, size_t size)
{
    (void)path;
    (void)buf;
    (void)size;
    return -int(errno_t::ENOSYS);
}

//
// Scan directories

int tmpfs_fs_t::opendir(fs_file_info_t **fi, fs_cpath_t path)
{
    (void)path;
    (void)fi;
    return -int(errno_t::ENOSYS);
}

ssize_t tmpfs_fs_t::readdir(fs_file_info_t *fi, dirent_t *buf, off_t offset)
{
    (void)buf;
    (void)offset;
    (void)fi;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::releasedir(fs_file_info_t *fi)
{
    (void)fi;
    return -int(errno_t::ENOSYS);
}


//
// Modify directories

int tmpfs_fs_t::mknod(fs_cpath_t path, fs_mode_t mode, fs_dev_t rdev)
{
    (void)path;
    (void)mode;
    (void)rdev;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::mkdir(fs_cpath_t path, fs_mode_t mode)
{
    (void)path;
    (void)mode;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::rmdir(fs_cpath_t path)
{
    (void)path;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::symlink(fs_cpath_t to, fs_cpath_t from)
{
    (void)to;
    (void)from;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::rename(fs_cpath_t from, fs_cpath_t to)
{
    (void)from;
    (void)to;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::link(fs_cpath_t from, fs_cpath_t to)
{
    (void)from;
    (void)to;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::unlink(fs_cpath_t path)
{
    (void)path;
    return -int(errno_t::ENOSYS);
}

//
// Modify directory entries

int tmpfs_fs_t::chmod(fs_cpath_t path, fs_mode_t mode)
{
    (void)path;
    (void)mode;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::chown(fs_cpath_t path, fs_uid_t uid, fs_gid_t gid)
{
    (void)path;
    (void)uid;
    (void)gid;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::truncate(fs_cpath_t path, off_t size)
{
    (void)path;
    (void)size;
    return -int(errno_t::ENOSYS);
}

int tmpfs_fs_t::utimens(fs_cpath_t path, const fs_timespec_t *ts)
{
    (void)path;
    (void)ts;
    return -int(errno_t::ENOSYS);
}

//
// Open/close files

struct tmpfs_file_t : public fs_file_info_t {
    size_t file_index;

    // fs_file_info_t interface
public:
    ino_t get_inode() const override
    {
        return file_index + 1;
    }
};

int tmpfs_fs_t::open(fs_file_info_t **fi,
                     fs_cpath_t path, int flags, mode_t mode)
{
    if (unlikely(flags & (O_CREAT | O_TRUNC)))
        return -int(errno_t::EROFS);

    size_t path_len = strlen(path);

    auto name_hash = hash_32(path, path_len);

    size_t index = 0;
    for (file_t const& file : files) {
        char const *name = names.data() + file.name_ofs;
        if (file.name_hash == name_hash &&
                file.name_sz == path_len + 1 &&
                !memcmp(name, path, path_len)) {
            std::unique_ptr<tmpfs_file_t> file(new tmpfs_file_t{});
            file->file_index = index;
            *fi = file.release();
            return 0;
        }
        ++index;
    }

    // Fixme
    (void)flags;
    (void)mode;

    return -int(errno_t::ENOENT);
}

int tmpfs_fs_t::release(fs_file_info_t *fi)
{
    delete fi;
    return 0;
}

//
// Read/write files

ssize_t tmpfs_fs_t::read(fs_file_info_t *fi, char *buf,
                                size_t size, off_t offset)
{
    auto file = static_cast<tmpfs_file_t*>(fi);
    auto const& file_info = files[file->file_index];

    if (offset >= file_info.file_sz)
        return 0;

    off_t avail = off_t(file_info.file_sz) - offset;

    if (off_t(size) > avail)
        size = avail;

    if (unlikely(off_t(size) < 0))
        return -int(errno_t::EINVAL);

    if (unlikely(offset < 0))
        return -int(errno_t::EINVAL);

    if (mm_is_user_range(buf, size))
        mm_copy_user(buf, st + file_info.file_ofs + offset, size);
    else
        memcpy(buf, st + file_info.file_ofs + offset, size);

    return size;
}

ssize_t tmpfs_fs_t::write(fs_file_info_t *fi, char const *buf,
                                 size_t size, off_t offset)
{
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    return -int(errno_t::EROFS);
}

int tmpfs_fs_t::ftruncate(fs_file_info_t *fi, off_t offset)
{
    (void)offset;
    (void)fi;
    // Fail, read only
    return -int(errno_t::EROFS);
}

//
// Query open files

int tmpfs_fs_t::fstat(fs_file_info_t *fi, fs_stat_t *st)
{
    auto file = static_cast<tmpfs_file_t*>(fi);
    auto const& file_info = files[file->file_index];
    *st = {};
    st->st_ino = file_info.ino;
    st->st_mode = file_info.mode;
    st->st_nlink = file_info.nlink;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = file_info.file_sz;
    st->st_blksize = 512;
    st->st_blocks = file_info.file_sz >> 9;
    st->st_atime = file_info.mtime;
    st->st_mtime = file_info.mtime;
    st->st_ctime = file_info.mtime;
    return 0;
}

//
// Sync files and directories and flush buffers

int tmpfs_fs_t::fsync(fs_file_info_t *fi, int isdatasync)
{
    (void)isdatasync;
    (void)fi;
    return 0;
}

int tmpfs_fs_t::fsyncdir(fs_file_info_t *fi, int isdatasync)
{
    return 0;
}

int tmpfs_fs_t::flush(fs_file_info_t *fi)
{
    return 0;
}

//
// Get filesystem information

int tmpfs_fs_t::statfs(fs_statvfs_t* stbuf)
{
    (void)stbuf;
    *stbuf = {};
    stbuf->f_bsize = 1;
    stbuf->f_namemax = 256;
    stbuf->f_bsize = en - st;
    stbuf->f_files = files.size();
    return 0;
}

//
// lock/unlock file

int tmpfs_fs_t::lock(
        fs_file_info_t *fi, int cmd, fs_flock_t* locks)
{
    (void)fi;
    (void)cmd;
    (void)locks;
    return -int(errno_t::ENOSYS);
}

//
// Get block map

int tmpfs_fs_t::bmap(
        fs_cpath_t path, size_t blocksize, uint64_t* blockno)
{
    (void)path;
    (void)blocksize;
    (void)blockno;
    return -int(errno_t::ENOSYS);
}

//
// Read/Write/Enumerate extended attributes

int tmpfs_fs_t::setxattr(
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

int tmpfs_fs_t::getxattr(
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

int tmpfs_fs_t::listxattr(
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

int tmpfs_fs_t::ioctl(
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

int tmpfs_fs_t::poll(fs_file_info_t *fi,
                            fs_pollhandle_t* ph,
                            unsigned* reventsp)
{
    (void)fi;
    (void)ph;
    (void)reventsp;
    return -int(errno_t::ENOSYS);
}

bool tmpfs_startup(void *st, size_t sz)
{
    initrd_st = st;
    initrd_sz = sz;

    std::unique_ptr<tmpfs_fs_t> fs(new tmpfs_fs_t);
    fs_init_info_t info{};
    info.part_st = (uint64_t)st;
    info.part_len = sz;
    auto mounted_fs = reinterpret_cast<fs_base_t*>(fs->mount(&info));
    if (unlikely(!mounted_fs))
        return false;

    fs_add(nullptr, mounted_fs);

    fs.release();

    return true;
}
