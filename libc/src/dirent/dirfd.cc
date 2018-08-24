#include <dirent.h>
#include <errno.h>
#include "bits/dirent.h"

int dirfd(DIR *__dirp)
{
    errno = ENOSYS;
    return -1;
}
