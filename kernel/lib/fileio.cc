#include "dev_storage.h"
#include "fileio.h"
#include "rbtree.h"
#include "callout.h"
#include "mm.h"
#include "stdlib.h"
#include "string.h"
#include "vector.h"

struct filetab_t {
    fs_file_info_t *fi;
    fs_base_t *fs;
    off_t pos;
    filetab_t *next_free;
    int refcount;
};

static spinlock file_table_lock;
static vector<filetab_t> file_table;
static filetab_t *file_table_ff;

static void file_init(void *)
{
    unique_lock<spinlock> lock(file_table_lock);
    file_table.reserve(1000);
}

static fs_base_t *file_fs_from_path(char const *path)
{
    (void)path;
    return fs_from_id(0);
}

static filetab_t *file_new_filetab(void)
{
    unique_lock<spinlock> lock(file_table_lock);
    filetab_t *item = nullptr;
    if (file_table_ff) {
        // Reuse freed item
        item = file_table_ff;
        file_table_ff = item->next_free;
        item->next_free = nullptr;
    } else if (file_table.size() < file_table.capacity()) {
        // Add another item
        file_table.emplace_back();
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
    if (--item->refcount == 0) {
        item->next_free = file_table_ff;
        file_table_ff = item;
        return true;
    }
    
    return false;
}

bool file_ref_filetab(int id)
{
    unique_lock<spinlock> lock(file_table_lock);
    if (id >= 0 && id < (int)file_table.size() &&
            file_table[id].refcount > 0) {
        ++file_table[id].refcount;
        return true;
    }
    
    return false;
}

REGISTER_CALLOUT(file_init, 0, callout_type_t::partition_probe, "999");

static filetab_t *file_fh_from_id(int id)
{
    filetab_t *item = nullptr;
    if (likely(id >= 0 && id < (int)file_table.size())) {
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
    fs_base_t *fs = file_fs_from_path(path);

    if (!fs)
        return -1;

    filetab_t *fh = file_new_filetab();

    int status = fs->open(&fh->fi, path, flags, mode);
    if (status < 0)
        return status;

    fh->fs = fs;
    //fh->path = strdup(path);
    fh->pos = 0;

    return fh - file_table.data();
}

int file_close(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    
    if (unlikely(!fh))
        return -1;
    
    if (file_del_filetab(fh)) {
        fh->fs->release(fh->fi);
        memset(fh, 0, sizeof(*fh));
    }

    return 0;
}

ssize_t file_pread(int id, void *buf, size_t bytes, off_t ofs)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    return fh->fs->read(fh->fi, (char*)buf, bytes, ofs);
}

ssize_t file_pwrite(int id, void const *buf, size_t bytes, off_t ofs)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    return fh->fs->write(fh->fi, (char*)buf, bytes, ofs);
}

int file_syncfs(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    return fh->fs->flush(fh->fi);
}

off_t file_seek(int id, off_t ofs, int whence)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    fs_stat_t st;

    switch (whence) {
    case SEEK_SET:
        if (ofs < 0)
            return -1;

        fh->pos = ofs;
        break;

    case SEEK_CUR:
        if (fh->pos + ofs < 0)
            return -1;

        fh->pos += ofs;
        break;

    case SEEK_HOLE: // fall through
    case SEEK_END:
        if (fh->fs->fstat(fh->fi, &st) < 0)
            return -1;

        if (st.st_size + ofs < 0)
            return -1;

        fh->pos = st.st_size + ofs;
        break;

    case SEEK_DATA:
        // Do nothing, everywhere is data
        break;

    default:
        return -1;
    }

    return fh->pos;
}

int file_ftruncate(int id, off_t size)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    return fh->fs->ftruncate(fh->fi, size);
}

ssize_t file_read(int id, void *buf, size_t bytes)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    ssize_t size = fh->fs->read(fh->fi, (char*)buf, bytes, fh->pos);
    if (size >= 0)
        fh->pos += size;

    return size;
}

ssize_t file_write(int id, void const *buf, size_t bytes)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    ssize_t size = fh->fs->write(fh->fi, (char*)buf, bytes, fh->pos);
    if (size >= 0)
        fh->pos += size;

    return size;
}

int file_fsync(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    return fh->fs->fsync(fh->fi, 0);
}

int file_fdatasync(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    return fh->fs->fsync(fh->fi, 1);
}

int file_opendir(char const *path)
{
    fs_base_t *fs = file_fs_from_path(path);

    if (!fs)
        return -1;

    filetab_t *fh = file_new_filetab();
    fh->fs = fs;

    int status = fh->fs->opendir(&fh->fi, path);
    if (status < 0)
        return -1;

    fh->fs = fs;
    //fh->path = strdup(path);
    fh->pos = 0;
    //fh->next = 0;

    return fh - file_table.data();
}

ssize_t file_readdir_r(int id, dirent_t *buf, dirent_t **result)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    ssize_t size = fh->fs->readdir(fh->fi, buf, fh->pos);

    *result = 0;
    if (size < 0)
        return -1;

    fh->pos += size;
    *result = buf;
    return size;
}

off_t file_telldir(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    return fh->pos;
}

off_t file_seekdir(int id, off_t ofs)
{
    filetab_t *fh = file_fh_from_id(id);
    if (!fh)
        return -1;

    fh->pos = ofs;
    return 0;
}

int file_closedir(int id)
{
    filetab_t *fh = file_fh_from_id(id);
    
    if (!fh)
        return -1;

    return fh->fs->releasedir(fh->fi);
}

int file_mkdir(char const *path, mode_t mode)
{
    fs_base_t *fs = file_fs_from_path(path);

    if (!fs)
        return -1;

    return fs->mkdir(path, mode);
}

int file_rmdir(char const *path)
{
    fs_base_t *fs = file_fs_from_path(path);

    if (!fs)
        return -1;

    return fs->rmdir(path);
}

int file_rename(char const *old_path, char const *new_path)
{
    fs_base_t *fs = file_fs_from_path(old_path);

    if (!fs)
        return -1;

    return fs->rename(old_path, new_path);
}

int file_unlink(char const *path)
{
    fs_base_t *fs = file_fs_from_path(path);

    if (!fs)
        return -1;

    return fs->unlink(path);
}
