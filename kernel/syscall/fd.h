#pragma once

#include "dirent.h"

extern "C" {

int sys_open(const char *pathname, int flags, mode_t mode);
ssize_t sys_read(int fd, void *bufaddr, size_t count);
ssize_t sys_write(int fd, void const *bufaddr, size_t count);
int sys_close(int fd);
off_t sys_lseek(int fd, off_t ofs, int whence);
ssize_t sys_pread64(int fd, void *bufaddr, 
                    size_t count, off_t ofs);
ssize_t sys_pwrite64(int fd, void const *bufaddr, 
                     size_t count, off_t ofs);


int sys_fsync(int fd);

int sys_fdatasync(int fd);

int sys_rename(char const *old_path, char const *new_path);

int sys_mkdir(char const *path, mode_t mode);

int sys_rmdir(char const *path);

int sys_unlink(char const *path);

int sys_ftruncate(int fd, off_t size);

int sys_creat(char const *path, mode_t mode);

int sys_dup(int oldfd);
int sys_dup2(int oldfd, int newfd);
int sys_dup3(int oldfd, int newfd, int flags);

}
