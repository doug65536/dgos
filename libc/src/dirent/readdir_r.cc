#include <dirent.h>
#include <errno.h>

int readdir_r(DIR *__dirp, struct dirent *__entry, struct dirent **result)
{
    errno = ENOSYS;
    return -1;
}
