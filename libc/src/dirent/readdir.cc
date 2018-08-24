#include <dirent.h>
#include <errno.h>
#include "bits/dirent.h"

struct dirent *readdir(DIR *__dirp)
{
    errno = ENOSYS;
    return nullptr;
}
