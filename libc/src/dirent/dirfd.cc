#include <dirent.h>
#include <errno.h>

int dirfd(DIR *__dirp)
{
    errno = ENOSYS;
    return -1;
}
