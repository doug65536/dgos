#include "dev_storage.h"
#include "fileio.h"
#include "basic_set.h"
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

    bool close_on_exec;
    bool async;
    bool nonblock;

    // Size align
    char reserved[25];
};

// Make it fast to compute index from pointer difference
C_ASSERT_ISPO2(sizeof(filetab_t));

using file_table_lock_type = ext::noirq_lock<ext::spinlock>;
using file_table_scoped_lock = ext::unique_lock<file_table_lock_type>;

static file_table_lock_type file_table_lock;

// Array of files
static ext::vector<filetab_t> file_table;

// First free
static filetab_t *file_table_ff;

static dev_fs_t *dev_fs;

static void file_init(void *)
{
    file_table_scoped_lock lock(file_table_lock);
    if (unlikely(!file_table.reserve(1000)))
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

static fs_base_t *file_fs_from_path(int dirid, char const *path,
                                    size_t& consumed)
{
    if (path[0] == '/' && !memcmp(path + consumed, "/dev/", 5)) {
        consumed += 5;

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

static filetab_t *file_new_filetab(void)
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
        if (unlikely(!file_table.emplace_back()))
            return nullptr;
        item = &file_table.back();
    } else {
        return nullptr;
    }
    assert(item->refcount == 0);
    item->fi = nullptr;
    item->fs = nullptr;
    item->pos = 0;
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

REGISTER_CALLOUT(file_init, nullptr,
                 callout_type_t::heap_ready, "000");

EXPORT int file_creatat(int dirid, char const *path, mode_t mode)
{
    return file_openat(dirid, path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

static int file_root_dirid()
{
    //  Get root filesystem
    fs_base_t *fs = fs_from_id(0);

    // Resolve root
    size_t consumed = 0;
    int dirid = fs->resolve(nullptr, nullptr, consumed);

    return dirid;
}

EXPORT int file_openat(int dirid, char const *path, int flags, mode_t mode)
{
    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(dirid, path, consumed);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    filetab_t *fh = file_new_filetab();

    if (unlikely(!fh))
        return -int(errno_t::ENFILE);

    filetab_t *dirfh = file_fh_from_id(dirid);

    int status = fs->openat(&fh->fi, dirfh ? dirfh->fi : nullptr,
                            path + consumed, flags, mode);

    if (unlikely(status < 0)) {
        FILEHANDLE_TRACE("open failed on %s, status=%d\n", path, status);
        file_del_filetab(fh);
        return status;
    }


    fh->fs = fs;
    fh->pos = 0;

    int id = fh - file_table.data();

    FILEHANDLE_TRACE("opened %s, fd=%d on %s filesystem\n", path, id,
                     fs->name());

    return id;
}

EXPORT int file_close(int id)
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

EXPORT ssize_t file_pread(int id, void *buf, size_t bytes, off_t ofs)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->read(fh->fi, (char*)buf, bytes, ofs);
}

EXPORT ssize_t file_pwrite(int id, void const *buf, size_t bytes, off_t ofs)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->write(fh->fi, (char*)buf, bytes, ofs);
}

EXPORT int file_syncfs(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->flush(fh->fi);
}

EXPORT off_t file_seek(int id, off_t ofs, int whence)
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

EXPORT int file_ftruncate(int id, off_t size)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->ftruncate(fh->fi, size);
}

EXPORT int file_ioctl(int id, int cmd, void* arg,
                      unsigned int flags, void* data)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->ioctl(fh->fi, cmd, arg, flags, data);
}

EXPORT int file_fstatfs(int id, fs_statvfs_t *buf)
{
    filetab_t *fh = file_fh_from_id(id);

    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->statfs(buf);
}

EXPORT ssize_t file_read(int id, void *buf, size_t bytes)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    ssize_t size = fh->fs->read(fh->fi, (char*)buf, bytes, fh->pos);
    if (size >= 0)
        fh->pos += size;

    return size;
}

EXPORT ssize_t file_write(int id, void const *buf, size_t bytes)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    ssize_t size = fh->fs->write(fh->fi, (char*)buf, bytes, fh->pos);
    if (size >= 0)
        fh->pos += size;

    return size;
}

EXPORT int file_fsync(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->fsync(fh->fi, 0);
}

EXPORT int file_fdatasync(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->fs->fsync(fh->fi, 1);
}

EXPORT int file_opendirat(int dirid, char const *path)
{
    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(dirid, path, consumed);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    filetab_t *fh = file_new_filetab();

    if (unlikely(!fh))
        return -int(errno_t::ENFILE);

    fh->fs = fs;

    filetab_t *dirfh = file_fh_from_id(dirid);

    int status = fh->fs->opendirat(&fh->fi, dirfh ? dirfh->fi : nullptr,
                                   path + consumed);
    if (unlikely(status < 0)) {
        file_del_filetab(fh);
        return status;
    }

    fh->fs = fs;
    fh->pos = 0;

    return fh - file_table.data();
}

EXPORT ssize_t file_readdir_r(int id, dirent_t *buf, dirent_t **result)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    ssize_t size = fh->fs->readdir(fh->fi, buf, fh->pos);

    *result = nullptr;
    if (unlikely(size < 0))
        return -int(errno_t::EINVAL);

    fh->pos += size;
    *result = buf;
    return size;
}

EXPORT off_t file_telldir(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    return fh->pos;
}

EXPORT off_t file_seekdir(int id, off_t ofs)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return -int(errno_t::EBADF);

    fh->pos = ofs;
    return 0;
}

EXPORT int file_closedir(int id)
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

EXPORT int file_mkdirat(int dirid, char const *path, mode_t mode)
{
    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(dirid, path, consumed);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    filetab_t *dirfh = file_fh_from_id(dirid);

    return fs->mkdirat(dirfh ? dirfh->fi : nullptr,
                       path + consumed, mode);
}

EXPORT int file_rmdirat(int dirid, char const *path)
{
    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(dirid, path, consumed);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    filetab_t *dirfh = file_fh_from_id(dirid);

    return fs->rmdirat(dirfh ? dirfh->fi : nullptr,
                       path + consumed);
}

EXPORT int file_renameat(int olddirid, char const *old_path,
                         int newdirid, char const *path)
{
    size_t old_consumed = 0;
    fs_base_t *old_fs = file_fs_from_path(olddirid, old_path, old_consumed);

    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(newdirid, path, consumed);

    if (unlikely(old_fs != fs))
        return -int(errno_t::EXDEV);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    filetab_t *olddirfh = file_fh_from_id(olddirid);
    filetab_t *newdirfh = file_fh_from_id(newdirid);

    return fs->renameat(olddirfh ? olddirfh->fi : nullptr,
                        old_path + old_consumed,
                        newdirfh ? newdirfh->fi : nullptr,
                        path + consumed);
}

EXPORT int file_unlinkat(int dirid, char const *path)
{
    size_t consumed = 0;
    fs_base_t *fs = file_fs_from_path(dirid, path, consumed);

    if (unlikely(!fs))
        return -int(errno_t::ENOENT);

    filetab_t *dirfh = file_fh_from_id(dirid);

    return fs->unlinkat(dirfh ? dirfh->fi : nullptr,
                        path + consumed);
}

EXPORT int file_fchmod(int id, mode_t mode)
{
    filetab_t *fh = file_fh_from_id(id);
    if (unlikely(!fh))
        return int(errno_t::EBADF);

    if (unlikely(!fh))
        return -int(errno_t::ENOENT);

    return fh->fs->fchmod(fh->fi, mode);
}

EXPORT int file_chown(int id, int uid, int gid)
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

ext::pair<char const *, char const *> path_t::range_of(size_t index) const
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

ext::string path_t::to_string() const
{
    ext::string s;

    s.reserve(8 + uintptr_t(components) - uintptr_t(&data));

    if (absolute)
        s.push_back('/');

    if (unc)
        s.push_back('/');

    for (size_t i = 0; i < nr_components; ++i) {
        if (i > 0)
            s.push_back('/');
        ext::pair<char const *, char const *> range = range_of(i);
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
    // Get a pointer to aligned storage
    char *buf = reinterpret_cast<char *>(&data);

    lenof_str = mm_copy_user_str(buf, user_str, PATH_MAX);

    // If failed to copy string
    if (unlikely(lenof_str < 0)) {
        set_err(errno_t::EFAULT);
        return;
    }

    char const *nt = (char const *)memchr(buf, 0, lenof_str + 1);

    // If the string myseriously changed length, fail
    // See if any null terminators sneaked into the string
    // Or the final null terminator mysteriously disappeared
    if (unlikely(nt != buf + lenof_str)) {
        // That's strange, nice try
        set_err(errno_t::EFAULT);
        return;
    }
}

user_str_t::user_str_t(const char *user_str, size_t truncate_len, truncate_t)
{
    truncate_len = ext::min(max_sz, truncate_len);
    lenof_str = 0;
    if (likely(mm_copy_user(data.data, user_str, truncate_len)))
        lenof_str = truncate_len;
    else
        lenof_str = -1;
}

// =======================================================================

class socket_fs_t final : public fs_nosys_t {
    //FS_BASE_RW_IMPL

    // fs_base_t interface
public:
    char const *name() const noexcept override { return "socket_fs"; }
    int openat(fs_file_info_t **fi, fs_file_info_t *dirfi,
               fs_cpath_t path, int flags, mode_t mode) override;
    int release(fs_file_info_t *fi) override;
    ssize_t read(fs_file_info_t *fi, char *buf,
                 size_t size, off_t offset) override;
    ssize_t write(fs_file_info_t *fi, const char *buf,
                  size_t size, off_t offset) override;
};

class socket_file_t : public fs_file_info_t {
public:


public:// fs_file_info_t interface
    ino_t get_inode() const override;
};

ino_t socket_file_t::get_inode() const
{
    return -ino_t(errno_t::ENOSYS);
}

int socket_fs_t::openat(fs_file_info_t **fi, fs_file_info_t *dirfi,
                        fs_cpath_t path, int flags, mode_t mode)
{
    socket_file_t *file = new (ext::nothrow) socket_file_t();
    *fi = file;

    return 0;
}

int socket_fs_t::release(fs_file_info_t *fi)
{
    socket_file_t *file = static_cast<socket_file_t*>(fi);
    delete file;
    return 0;
}

ssize_t socket_fs_t::read(fs_file_info_t *fi,
                          char *buf, size_t size, off_t offset)
{
    return 0;
}

ssize_t socket_fs_t::write(fs_file_info_t *fi,
                           const char *buf, size_t size, off_t offset)
{
    return 0;
}

socket_fs_t socket_fs;

int file_create_socket()
{
    filetab_t *fh = file_new_filetab();

    if (unlikely(!fh))
        return -int(errno_t::ENFILE);

    fh->fs = &socket_fs;
    fh->pos = 0;

    int id = fh - file_table.data();

    FILEHANDLE_TRACE("opened socket, fd=%d\n", id);

    return id;
}
