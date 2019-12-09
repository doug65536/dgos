#include <dirent.h>
#include <errno.h>
#include <sys/likely.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include "bits/dirent.h"

static __thread dirent_t current_dirent;

int readdir_r(int dirfd, dirent_t *buf, dirent_t **result)
{
    long err = syscall2(long(dirfd), long(buf), SYS_readdir_r);

    if (unlikely(err < 0)) {
        errno = -err;
        return -1;
    }

    *result = buf;

    return int(err);
}

struct dirent *readdir(DIR *__dirp)
{
    dirent_t *result = nullptr;
    dirent_t *buf = &current_dirent;

    if (unlikely(readdir_r(__dirp->dirfd, buf, &result) == 0))
        return nullptr;

    return result;
}
