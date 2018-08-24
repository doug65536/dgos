#include <dirent.h>
#include <errno.h>
#include "bits/dirent.h"

DIR *fdopendir(int __dirfd)
{
    errno = ENOSYS;
    return nullptr;
}
