#pragma once
#include "types.h"

typedef uint64_t ino_t;
typedef uint16_t mode_t;

struct dirent {
    ino_t d_ino;       /* inode number */
    char     d_name[256]; /* filename */
};

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2
#define SEEK_DATA   3
#define SEEK_HOLE   4

typedef struct dirent dirent;

int file_open(char const *path);
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

int file_opendir(char *path);
ssize_t file_readdir_r(int fd, dirent *buf, dirent **result);
off_t file_telldir(int fd);
off_t file_seekdir(int fd, off_t ofs);
int file_closedir(int fd);

int file_mkdir(char const *path, mode_t mode);
int file_rmdir(char const *path);
int file_rename(char const *old_path, char const *new_path);
int file_unlink(char const *path);

void file_autoclose(int *fd);

#define autoclose __attribute__((cleanup(file_autoclose)))
