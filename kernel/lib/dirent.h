#pragma once
#include "types.h"

typedef uint64_t ino_t;
typedef uint16_t mode_t;

typedef struct dirent_t {
    ino_t d_ino;
    char     d_name[256];
} dirent_t;

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
