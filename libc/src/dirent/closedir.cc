#include <dirent.h>
#include <errno.h>

int closedir(DIR *__dirp)
{
    errno = ENOSYS;
    return -1;
}
