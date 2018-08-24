#include <dirent.h>
#include <errno.h>
#include "bits/dirent.h"

void seekdir(DIR *__dirp, off_t __ofs)
{
    errno = ENOSYS;
}
