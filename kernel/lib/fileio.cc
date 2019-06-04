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
#include "user_mem.h"

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

REGISTER_CALLOUT(file_init, nullptr, callout_type_t::heap_ready, "000");

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

int file_fchmod(int id, mode_t mode)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return int(errno_t::EBADF);

    if (unlikely(!fh))
        return -int(errno_t::ENOENT);

    return fh->fs->fchmod(fh->fi, mode);
}

int file_chown(int id, int uid, int gid)
{
    filetab_t *fh = file_fh_from_id(id);

    if (unlikely(!fh))
        return -int(errno_t::ENOENT);

    return fh->fs->fchown(fh->fi, uid, gid);
}

path_t::path_t(char const *user_path)
{
    // Get the length of the user memory string
    intptr_t len = mm_lenof_user_str(user_path, PATH_MAX);

    // Return with valid still false
    if (unlikely(len < 0))
        return;

    // Get a pointer to aligned storage
    char *buf = reinterpret_cast<char *>(&data);

    // If failed to copy string
    if (unlikely(!mm_copy_user(buf, user_path, len + 1))) {
        err = errno_t::EFAULT;
        return;
    }

    // If the string myseriously changed length, fail
    if (unlikely(memchr(buf, 0, len + 1) != buf + len)) {
        // That's strange, nice try
        err = errno_t::EFAULT;
        return;
    }

    // Round up to aligned boundary and place component pointer list there
    size_t comp_ofs = (len + sizeof(void*) - 1) & -(sizeof(void*));

    components = (path_frag_t *)(buf + comp_ofs);

    char const *it = buf;

    // One or three (but not two) leading slashes means absolute
    if ((it[0] == '/' && it[1] != '/') ||
            (it[0] == '/' && it[1] == '/' && it[2] == '/')) {
        // Absolute path
        absolute = true;
        while (*++it == '/');
    } else if (it[0] == '/' && it[1] == '/') {
        // UNC path
        absolute = true;
        unc = true;
        it += 2;
    }

    char const *st = it;

    for (; ; ) {
        // Handle separator or end of path as end of token
        if (*it != '/' && *it != 0) {
            ++it;
        } else {
            // Emit a component
            components[nr_components++] = {
                uint16_t(st - buf), uint16_t(it - buf), hash_32(st, it - st)
            };

            // If it was end of path, then done
            if (*it == 0)
                break;

            // Eat consecutive slashes
            while (*++it == '/');

            // Next token starts here
            st = it;
        }
    }

    valid = true;
}

size_t path_t::size() const
{
    return nr_components;
}

size_t path_t::text_len() const
{
    return uintptr_t(components) - uintptr_t(&data);
}

uint32_t path_t::hash_of(size_t index) const
{
    return components[index].hash;
}

char const *path_t::begin_of(size_t index) const
{
    if (likely(index < nr_components))
        return reinterpret_cast<char const *>(data.data) +
                components[index].st;
    assert(!"Not good");
    return nullptr;
}

char const *path_t::end_of(size_t index) const
{
    if (likely(index < nr_components))
        return reinterpret_cast<char const *>(data.data) +
                components[index].en;
    assert(!"Not good");
    return nullptr;
}

size_t path_t::len_of(size_t index) const
{
    if (likely(index < nr_components)) {
        path_frag_t const& frag = components[index];
        return frag.en - frag.st;
    }
    return 0;
}

std::pair<char const *, char const *> path_t::range_of(size_t index) const
{
    if (likely(index < nr_components)) {
        path_frag_t const& frag = components[index];
        return {
            reinterpret_cast<char const *>(data.data) + frag.st,
                    reinterpret_cast<char const *>(data.data) + frag.en
        };
    }
    assert(!"Not good");
    return {nullptr, nullptr};
}

bool path_t::is_abs() const
{
    return absolute;
}

bool path_t::is_unc() const
{
    return unc;
}

bool path_t::is_relative() const
{
    return !absolute;
}

bool path_t::is_valid() const
{
    return valid;
}

errno_t path_t::error() const
{
    return err;
}

char const *path_t::operator[](size_t component) const
{
    return begin_of(component);
}

std::string path_t::to_string() const
{
    std::string s;

    s.reserve(8 + uintptr_t(components) - uintptr_t(&data));

    if (absolute)
        s.push_back('/');

    if (unc)
        s.push_back('/');

    for (size_t i = 0; i < nr_components; ++i) {
        if (i > 0)
            s.push_back('/');
        std::pair<char const *, char const *> range = range_of(i);
        s.append(range.first, range.second);
    }

    return s;
}

path_t::operator bool() const
{
    return valid && err == errno_t::OK;
}

user_str_t::user_str_t(const char *user_str)
{
    // Get the length of the user memory string
    lenof_str = mm_lenof_user_str(user_str, PATH_MAX);

    // Return with valid still false
    if (unlikely(lenof_str < 0))
        return;

    // Get a pointer to aligned storage
    char *buf = reinterpret_cast<char *>(&data);

    // If failed to copy string
    if (unlikely(!mm_copy_user(buf, user_str, lenof_str + 1))) {
        set_err(errno_t::EFAULT);
        return;
    }

    // If the string myseriously changed length, fail
    // See if any null terminators sneaked into the string
    // Or the final null terminator mysteriously disappeared
    if (unlikely(memchr(buf, 0, lenof_str + 1) != buf + lenof_str)) {
        // That's strange, nice try
        set_err(errno_t::EFAULT);
        return;
    }
}
