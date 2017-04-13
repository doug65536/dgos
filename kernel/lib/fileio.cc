#include "dev_storage.h"
#include "fileio.h"
#include "rbtree.h"
#include "callout.h"
#include "mm.h"
#include "stdlib.h"
#include "string.h"

typedef struct file_handle_t file_handle_t;
struct file_handle_t {
    file_handle_t *next;
    fs_file_info_t *fi;
    char *path;
    fs_base_t *fs;
    off_t pos;
};

static file_handle_t *files;
static size_t file_capacity;

static void file_init(void *p)
{
    (void)p;
    file_capacity = 16;
    files = (file_handle_t*)mmap(0, sizeof(file_handle_t) * file_capacity,
                 PROT_READ | PROT_WRITE,
                 0, -1, 0);
}

static fs_base_t *file_fs_from_path(char const *path)
{
    (void)path;
    return fs_from_id(0);
}

static file_handle_t *file_new_fd(void)
{
    for (size_t i = 1; i < file_capacity; ++i)
        if (!files[i].fs)
            return (file_handle_t*)memset(files + i, 0, sizeof(*files));
    return 0;
}

REGISTER_CALLOUT(file_init, 0, 'P', "999");

static file_handle_t *file_fh_from_fd(int fd)
{
    return fd > 0 &&
            (size_t)fd < file_capacity &&
            files[fd].fs
            ? files + fd
            : 0;
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

    file_handle_t *fh = file_new_fd();

    int status = fs->open(&fh->fi, path, flags, mode);
    if (status < 0)
        return -1;

    fh->fs = fs;
    fh->next = 0;
    fh->path = strdup(path);
    fh->pos = 0;

    return fh - files;
}

int file_close(int fd)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    fh->fs->release(fh->fi);

    free(fh->path);

    memset(fh, 0, sizeof(*fh));

    return 0;
}

ssize_t file_pread(int fd, void *buf, size_t bytes, off_t ofs)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    return fh->fs->read(fh->fi, (char*)buf, bytes, ofs);
}

ssize_t file_pwrite(int fd, void *buf, size_t bytes, off_t ofs)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    return fh->fs->write(fh->fi, (char*)buf, bytes, ofs);
}

int file_syncfs(int fd)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    return fh->fs->flush(fh->fi);
}

off_t file_seek(int fd, off_t ofs, int whence)
{
    file_handle_t *fh = file_fh_from_fd(fd);
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

int file_ftruncate(int fd, off_t size)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    return fh->fs->ftruncate(fh->fi, size);
}

ssize_t file_read(int fd, void *buf, size_t bytes)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    ssize_t size = fh->fs->read(fh->fi, (char*)buf, bytes, fh->pos);
    if (size >= 0)
        fh->pos += size;

    return size;
}

ssize_t file_write(int fd, void const *buf, size_t bytes)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    ssize_t size = fh->fs->write(fh->fi, (char*)buf, bytes, fh->pos);
    if (size >= 0)
        fh->pos += size;

    return size;
}

int file_sync(int fd)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    return fh->fs->fsync(fh->fi, 0);
}

int file_datasync(int fd)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    return fh->fs->fsync(fh->fi, 1);
}

int file_opendir(char const *path)
{
    fs_base_t *fs = file_fs_from_path(path);

    if (!fs)
        return -1;

    file_handle_t *fh = file_new_fd();
    fh->fs = fs;

    int status = fh->fs->opendir(&fh->fi, path);
    if (status < 0)
        return -1;

    fh->fs = fs;
    fh->path = strdup(path);
    fh->pos = 0;
    fh->next = 0;

    return fh - files;
}

ssize_t file_readdir_r(int fd, dirent_t *buf, dirent_t **result)
{
    file_handle_t *fh = file_fh_from_fd(fd);
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

off_t file_telldir(int fd)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    return fh->pos;
}

off_t file_seekdir(int fd, off_t ofs)
{
    file_handle_t *fh = file_fh_from_fd(fd);
    if (!fh)
        return -1;

    fh->pos = ofs;
    return 0;
}

int file_closedir(int fd)
{
    file_handle_t *fh = file_fh_from_fd(fd);
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

void file_autoclose(int *fd)
{
    if (*fd > 0) {
        file_close(*fd);
        *fd = 0;
    }
}
