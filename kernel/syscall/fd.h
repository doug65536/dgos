#pragma once

#include "dirent.h"

extern "C" {

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
int sys_dup(int oldfd);
int sys_dup2(int oldfd, int newfd);
int sys_dup3(int oldfd, int newfd, int flags);
int sys_ftruncate(int fd, off_t size);

int sys_open(const char *pathname, int flags, mode_t mode);
int sys_creat(char const *path, mode_t mode);
int sys_rename(char const *old_path, char const *new_path);
int sys_mkdir(char const *path, mode_t mode);
int sys_rmdir(char const *path);
int sys_unlink(char const *path);
int sys_truncate(char const *path, off_t size);
int sys_access(char const *path, int mask);

int sys_mknod(char const *path, mode_t mode, int rdev);

int sys_link(char const *from, char const *to);
int sys_chmod(char const *path, mode_t mode);
int sys_fchmod(int fd, mode_t mode);
int sys_chown(char const *path, int uid, int gid);
int sys_fchown(int fd, int uid, int gid);

int sys_setxattr(char const *path,
        char const* name, char const* value,
        size_t size, int flags);
int sys_getxattr(char const *path,
        char const* name, char* value, size_t size);
int sys_listxattr(char const *path,
         char const* list, size_t size);

//int opendir(fs_file_info_t **fi, fs_cpath_t path) final;            
//ssize_t readdir(fs_file_info_t *fi, dirent_t* buf,                  
//                off_t offset) final;                                
//int releasedir(fs_file_info_t *fi) final;                           
//int getattr(fs_cpath_t path, fs_stat_t* stbuf) final;               
//int readlink(fs_cpath_t path, char* buf, size_t size) final;        
//int symlink(fs_cpath_t to, fs_cpath_t from) final;                  
//int utimens(fs_cpath_t path,                                        
//       fs_timespec_t const *ts) final;                              
//int release(fs_file_info_t *fi) final;                              
//int fstat(fs_file_info_t *fi,                                       
//     fs_stat_t *st) final;                                          
//int fsyncdir(fs_file_info_t *fi,                                    
//        int isdatasync) final;                                      
//int flush(fs_file_info_t *fi) final;                                
//int lock(fs_file_info_t *fi,                                        
//    int cmd, fs_flock_t* locks) final;                              
//int bmap(fs_cpath_t path, size_t blocksize,                         
//    uint64_t* blockno) final;                                       
//int statfs(fs_statvfs_t* stbuf) final;                              
//int ioctl(fs_file_info_t *fi,                                       
//     int cmd, void* arg,                                            
//     unsigned int flags, void* data) final;                         
//int poll(fs_file_info_t *fi,                                        
//    fs_pollhandle_t* ph, unsigned* reventsp) final;

}
