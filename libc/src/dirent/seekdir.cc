#include <dirent.h>
#include <errno.h>

void seekdir(DIR *__dirp, off_t __ofs)
{
    errno = ENOSYS;
}
