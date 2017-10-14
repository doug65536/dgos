#pragma once
#include "types.h"
#include "dirent.h"

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2
#define SEEK_DATA   3
#define SEEK_HOLE   4

int file_creat(char const *path, mode_t mode);
int file_open(char const *path, int flags, mode_t mode = 0);
int file_close(int fd);
ssize_t file_read(int fd, void *buf, size_t bytes);
ssize_t file_write(int fd, void const *buf, size_t bytes);
ssize_t file_pread(int fd, void *buf, size_t bytes, off_t ofs);
ssize_t file_pwrite(int fd, void *buf, size_t bytes, off_t ofs);
off_t file_seek(int fd, off_t ofs, int whence);
int file_ftruncate(int fd, off_t size);
int file_sync(int fd);
int file_datasync(int fd);
int file_syncfs(int fd);

int file_opendir(char const *path);
ssize_t file_readdir_r(int fd, dirent_t *buf, dirent_t **result);
off_t file_telldir(int fd);
off_t file_seekdir(int fd, off_t ofs);
int file_closedir(int fd);

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
    
    file_t(int fd)
        : fd(fd)
    {
    }
    
    ~file_t()
    {
        if (fd >= 0)
            file_close(fd);
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
    
private:
    int fd;
};
