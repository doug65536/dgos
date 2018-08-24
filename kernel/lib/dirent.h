#pragma once
#include "types.h"

typedef uint64_t ino_t;
typedef uint16_t mode_t;

struct dirent_t {
    ino_t d_ino;
    char d_name[256];
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

#define O_RDONLY    (1<<0)
#define O_WRONLY    (1<<1)
#define O_RDWR      (O_RDONLY|O_WRONLY)
#define O_APPEND    (1<<2)
#define O_ASYNC     (1<<3)
#define O_CLOEXEC   (1<<4)
#define O_CREAT     (1<<5)
#define O_DIRECT    (1<<6)
#define O_DIRECTORY (1<<7)
#define O_DSYNC     (1<<8)
#define O_EXCL      (1<<9)
#define O_LARGEFILE (1<<10)
#define O_NOATIME   (1<<11)
#define O_NOCTTY    (1<<12)
#define O_NOFOLLOW  (1<<13)
#define O_NBLOCK    (1<<14)
#define O_NDELAY    O_NBLOCK
#define O_PATH      (1<<15)
#define O_SYNC      (1<<16)
#define O_TMPFILE   (1<<17)
#define O_TRUNC     (1<<18)
