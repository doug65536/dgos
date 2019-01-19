#pragma once
#include "types.h"
#include "dirent.h"

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2
#define SEEK_DATA   3
#define SEEK_HOLE   4

bool file_ref_filetab(int id);

int file_creat(char const *path, mode_t mode);
int file_open(char const *path, int flags, mode_t mode = 0);
int file_close(int id);
ssize_t file_read(int id, void *buf, size_t bytes);
ssize_t file_write(int id, void const *buf, size_t bytes);
ssize_t file_pread(int id, void *buf, size_t bytes, off_t ofs);
ssize_t file_pwrite(int id, const void *buf, size_t bytes, off_t ofs);
off_t file_seek(int id, off_t ofs, int whence);
int file_ftruncate(int id, off_t size);
int file_fsync(int id);
int file_fdatasync(int id);
int file_syncfs(int id);
int file_ioctl(int id, int cmd, void* arg, unsigned flags, void *data);

int file_opendir(char const *path);
ssize_t file_readdir_r(int id, dirent_t *buf, dirent_t **result);
off_t file_telldir(int id);
off_t file_seekdir(int id, off_t ofs);
int file_closedir(int id);

int file_mkdir(char const *path, mode_t mode);
int file_rmdir(char const *path);
int file_rename(char const *old_path, char const *new_path);
int file_unlink(char const *path);

class file_t {
public:
    file_t()
        : fd(-1)
    {
    }

    explicit file_t(int fd)
        : fd(fd)
    {
    }

    file_t(file_t&& rhs)
        : fd(rhs.fd)
    {
        rhs.fd = -1;
    }

    file_t(file_t const&) = delete;
    file_t& operator=(file_t const&) = delete;

    file_t& operator=(file_t rhs)
    {
        if (fd != rhs.fd)
            close();
        fd = rhs.fd;
        rhs.fd = -1;
        return *this;
    }

    ~file_t()
    {
        close();
    }

    int release()
    {
        int result = fd;
        fd = -1;
        return result;
    }

    int close()
    {
        if (fd >= 0) {
            int result = file_close(fd);
            fd = -1;
            return result;
        }
        return 0;
    }

    file_t& operator=(int fd)
    {
        close();
        this->fd = fd;
        return *this;
    }

    operator int() const
    {
        return fd;
    }

    bool is_open() const
    {
        return fd >= 0;
    }

    operator bool() const
    {
        return is_open();
    }

private:
    int fd;
};

