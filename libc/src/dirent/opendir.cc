#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include "bits/dirent.h"
#include <sys/likely.h>

int _opendir(char const *__pathname)
{
    return (int)syscall2(AT_FDCWD, uintptr_t(__pathname), SYS_opendir);
}

DIR *opendir(char const *__pathname)
{
    DIR *dir = new DIR;
    if (unlikely(dir == nullptr)) {
        errno = ENOMEM;
        return nullptr;
    }

    dir->dirfd = _opendir(__pathname);
    return dir;
}
