#pragma once

#include <sys/types.h>

__BEGIN_DECLS

typedef struct __dirstream DIR;

// d_namlen member exists, it contains the size of the name
#define _DIRENT_HAVE_D_NAMLEN

// d_reclen member exists, it contains the size of the entire directory entry
#define _DIRENT_HAVE_D_RECLEN

// d_off member exists, it contains the file offset of the next directory entry
#define _DIRENT_HAVE_D_OFF

// d_type member exists, it contains the type of the file.
#define _DIRENT_HAVE_D_TYPE

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

struct dirent {
    ino_t d_ino;
    off_t d_off;

    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[NAME_MAX+1];
};

DIR *opendir(char const *__pathname);
DIR *fdopendir(int __dirfd);
int closedir(DIR *__dirp);
int dirfd(DIR *__dirp);
struct dirent *readdir(DIR *__dirp);
void rewinddir(DIR *__dirp);
int readdir_r(DIR *__dirp, struct dirent *__entry, struct dirent **result);
void seekdir(DIR *__dirp, off_t __ofs);
off_t telldir(DIR *__dirp);

__END_DECLS
