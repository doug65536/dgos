#include <dirent.h>
#include <errno.h>

DIR *fdopendir(int __dirfd)
{
    errno = ENOSYS;
    return nullptr;
}
