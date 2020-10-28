#pragma once
#include "types.h"
#include "syscall/sys_limits.h"

typedef uint64_t ino_t;
typedef uint16_t mode_t;

struct dirent_t {
    ino_t d_ino;
    off_t d_off;

    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[NAME_MAX+1];
};

//
// mode_t values

#define S_IRWXU  00700 // user mask
#define S_IRUSR  00400 // user has read permission
#define S_IWUSR  00200 // user has write permission
#define S_IXUSR  00100 // user has execute permission
#define S_IRWXG  00070 // group mask
#define S_IRGRP  00040 // group has read permission
#define S_IWGRP  00020 // group has write permission
#define S_IXGRP  00010 // group has execute permission
#define S_IRWXO  00007 // others mask
#define S_IROTH  00004 // others have read permission
#define S_IWOTH  00002 // others have write permission
#define S_IXOTH  00001 // others have execute permission

#define S_ISUID  04000 // set-user-ID bit
#define S_ISGID  02000 // set-group-ID bit (see stat(2))
#define S_ISVTX  01000 // sticky bit (see stat(2))

//
// open flags

#define O_RDONLY    0x1
#define O_WRONLY    0x2
#define O_RDWR      (O_RDONLY|O_WRONLY)
#define O_APPEND    0x4
#define O_ASYNC     0x8
#define O_CLOEXEC   0x10
#define O_CREAT     0x20
#define O_DIRECT    0x40
#define O_DIRECTORY 0x80
#define O_DSYNC     0x100
#define O_EXCL      0x200
#define O_LARGEFILE 0x400
#define O_NOATIME   0x800
#define O_NOCTTY    0x1000
#define O_NOFOLLOW  0x2000
#define O_NBLOCK    0x4000
#define O_PATH      0x8000
#define O_SYNC      0x10000
#define O_TMPFILE   0x20000
#define O_TRUNC     0x40000
#define O_EXEC      0x80000
#define O_NDELAY    O_NBLOCK
