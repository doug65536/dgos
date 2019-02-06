#include "dev_storage.h"
#include "fileio.h"
#include "rbtree.h"
#include "callout.h"
#include "mm.h"
#include "stdlib.h"
#include "string.h"
#include "vector.h"
#include "mutex.h"
#include "fs/devfs.h"

#define DEBUG_FILEHANDLE 1
#if DEBUG_FILEHANDLE
#define FILEHANDLE_TRACE(...) printdbg("filehandle: " __VA_ARGS__)
#else
#define FILEHANDLE_TRACE(...) ((void)0)
#endif

struct filetab_t {
    // Default and move construct allowed
    filetab_t() = default;
    filetab_t(filetab_t&&) = default;

    // Copying not allowed
    filetab_t(filetab_t const&) = delete;
    filetab_t& operator=(filetab_t const&) = delete;
    filetab_t& operator=(filetab_t&&) = delete;

    // Filesystem specific per-file structure
    fs_file_info_t *fi;

    // Filesystem implementation
    fs_base_t *fs;

    // Current seek position
    off_t pos;

    // Free list pointer
    filetab_t *next_free;

    // Reference count
    int refcount;

    // Size align
    int reserved[7];
};

// Make it fast to compute index from pointer difference
C_ASSERT_ISPO2(sizeof(filetab_t));

using file_table_lock_type = ext::mcslock;
using file_table_scoped_lock = std::unique_lock<file_table_lock_type>;

static file_table_lock_type file_table_lock;

// Array of files
static std::vector<filetab_t> file_table;

// First free
static filetab_t *file_table_ff;

static dev_fs_t *dev_fs;

static void file_init(void *)
{
    file_table_scoped_lock lock(file_table_lock);
    if (!file_table.reserve(1000))
        panic_oom();
}

static fs_base_t *file_fs_from_path(char const *path, size_t& consumed)
{
    if (path[0] == '/' && !memcmp(path, "/dev/", 5)) {
        consumed = 5;

        auto cur_devfs = atomic_ld_acq(&dev_fs);

        if (cur_devfs)
            return cur_devfs;

        // Create an instance, possibly racing with another thread
        auto* new_devfs = devfs_create();

        // Set it if it was zero
        auto old_devfs = atomic_cmpxchg(&dev_fs, nullptr, new_devfs);

        if (unlikely(old_devfs != nullptr)) {
            // We got nonzero, another thread won the race, cleanup
            devfs_delete(new_devfs);
            return old_devfs;
        }

        return new_devfs;
    }

    return fs_from_id(0);
}

filetab_t *file_new_filetab(void)
{
    file_table_scoped_lock lock(file_table_lock);
    filetab_t *item = nullptr;
    if (file_table_ff) {
        // Reuse freed item
        item = file_table_ff;
        file_table_ff = item->next_free;
        item->next_free = nullptr;
    } else if (file_table.size() < file_table.capacity()) {
        // Add another item
        if (!file_table.emplace_back())
            return nullptr;
        item = &file_table.back();
    } else {
        return nullptr;
    }
    assert(item->refcount == 0);
    item->refcount = 1;
    return item;
}

static bool file_del_filetab(filetab_t *item)
{
    file_table_scoped_lock lock(file_table_lock);
    assert(item->refcount != 0);
    if (--item->refcount == 0) {
        item->next_free = file_table_ff;
        file_table_ff = item;
        return true;
    }

    return false;
}

bool file_ref_filetab(int id)
{
    file_table_scoped_lock lock(file_table_lock);
    if (id >= 0 && size_t(id) < file_table.size()) {
        assert(file_table[id].refcount > 0);
        ++file_table[id].refcount;
        return true;
    }

    return false;
}

REGISTER_CALLOUT(file_init, nullptr, callout_type_t::partition_probe, "999");

static filetab_t *file_fh_from_id(int id)
{
    file_table_scoped_lock lock(file_table_lock);
    filetab_t *item = nullptr;
    if (likely(id >= 0 && size_t(id) < file_table.size())) {
        item = &file_table[id];

        if (likely(item->refcount > 0))
            return item;
        assert(!"Zero ref count!");
    }
    return nullptr;
}

int file_creat(char const *path, mode_t mode)
{
    return file_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int file_open(char const *path, int flags, mode_t mode)
{
    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(path, consumed);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    filetab_t *fh = file_new_filetab();

    int id = fh - file_table.data();

    if (unlikely(!fh))
        return -int(errno_t::ENFILE);

    int status = fs->open(&fh->fi, path + consumed, flags, mode);
    if (unlikely(status < 0)) {
        FILEHANDLE_TRACE("open failed on %s, status=%d\n", path, status);
        file_del_filetab(fh);
        return status;
    }

    FILEHANDLE_TRACE("opened %s, fd=%d\n", path, id);

    fh->fs = fs;
    fh->pos = 0;

    return id;
}

int file_close(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    int status = 0;

    fs_file_info_t *saved_file_info = fh->fi;
    if (file_del_filetab(fh))
        status = fh->fs->release(saved_file_info);

    FILEHANDLE_TRACE("closed fd=%d\n", id);

    return status;
}

ssize_t file_pread(int id, void *buf, size_t bytes, off_t ofs)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->read(fh->fi, (char*)buf, bytes, ofs);
}

ssize_t file_pwrite(int id, void const *buf, size_t bytes, off_t ofs)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->write(fh->fi, (char*)buf, bytes, ofs);
}

int file_syncfs(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->flush(fh->fi);
}

off_t file_seek(int id, off_t ofs, int whence)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    int status;
    fs_stat_t st;

    switch (whence) {
    case SEEK_SET:
        if (unlikely(ofs < 0))
            return -int(errno_t::EINVAL);

        fh->pos = ofs;
        break;

    case SEEK_CUR:
        if (unlikely(fh->pos + ofs < 0))
            return -int(errno_t::EINVAL);

        fh->pos += ofs;
        break;

    case SEEK_HOLE: // fall through
    case SEEK_END:
        status = fh->fs->fstat(fh->fi, &st);
        if (unlikely(status < 0))
            return status;

        if (st.st_size + ofs < 0)
            return -int(errno_t::EINVAL);

        fh->pos = st.st_size + ofs;
        break;

    case SEEK_DATA:
        // Do nothing, everywhere is data
        break;

    default:
        return -int(errno_t::EINVAL);

    }

    return fh->pos;
}

int file_ftruncate(int id, off_t size)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->ftruncate(fh->fi, size);
}

int file_ioctl(int id, int cmd, void* arg, unsigned int flags, void* data)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->ioctl(fh->fi, cmd, arg, flags, data);
}

ssize_t file_read(int id, void *buf, size_t bytes)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    ssize_t size = fh->fs->read(fh->fi, (char*)buf, bytes, fh->pos);
    if (size >= 0)
        fh->pos += size;

    return size;
}

ssize_t file_write(int id, void const *buf, size_t bytes)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    ssize_t size = fh->fs->write(fh->fi, (char*)buf, bytes, fh->pos);
    if (size >= 0)
        fh->pos += size;

    return size;
}

int file_fsync(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->fsync(fh->fi, 0);
}

int file_fdatasync(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->fsync(fh->fi, 1);
}

int file_opendir(char const *path)
{
    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(path, consumed);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    filetab_t *fh = file_new_filetab();

    if (unlikely(!fh))
        return -int(errno_t::ENFILE);

    fh->fs = fs;

    int status = fh->fs->opendir(&fh->fi, path);
    if (unlikely(status < 0)) {
        file_del_filetab(fh);
        return status;
    }

    fh->fs = fs;
    fh->pos = 0;

    return fh - file_table.data();
}

ssize_t file_readdir_r(int id, dirent_t *buf, dirent_t **result)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    ssize_t size = fh->fs->readdir(fh->fi, buf, fh->pos);

    *result = nullptr;
    if (size < 0)
        return -int(errno_t::EINVAL);

    fh->pos += size;
    *result = buf;
    return size;
}

off_t file_telldir(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->pos;
}

off_t file_seekdir(int id, off_t ofs)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    fh->pos = ofs;
    return 0;
}

int file_closedir(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    int status = 0;

    fs_file_info_t *saved_file_info = fh->fi;
    if (file_del_filetab(fh))
        status = fh->fs->releasedir(saved_file_info);

    return status;
}

int file_mkdir(char const *path, mode_t mode)
{
    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(path, consumed);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    return fs->mkdir(path + consumed, mode);
}

int file_rmdir(char const *path)
{
    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(path, consumed);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    return fs->rmdir(path + consumed);
}

int file_rename(char const *old_path, char const *path)
{
    size_t old_consumed = 0;
    fs_base_t *old_fs = file_fs_from_path(old_path, old_consumed);

    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(path, consumed);

    if (unlikely(old_fs != fs))
        return -int(errno_t::EXDEV);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    return fs->rename(old_path + old_consumed, path + consumed);
}

int file_unlink(char const *path)
{
    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(path, consumed);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    return fs->unlink(path + consumed);
}
