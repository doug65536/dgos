#include <dirent.h>
#include <errno.h>
#include "bits/dirent.h"

int closedir(DIR *__dirp)
{
    __dirp->dirfd = -1;
    delete __dirp;
    return 0;
}
