#include <dirent.h>
#include <errno.h>

off_t telldir(DIR *__dirp)
{
    errno = ENOSYS;
    return -1;
}
