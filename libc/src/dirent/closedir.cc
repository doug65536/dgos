#include <dirent.h>
#include <errno.h>
#include "bits/dirent.h"

int closedir(DIR *__dirp)
{
    errno = ENOSYS;
    return -1;
}
