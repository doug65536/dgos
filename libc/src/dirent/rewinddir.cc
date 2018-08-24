#include <dirent.h>
#include <errno.h>
#include "bits/dirent.h"

void rewinddir(DIR *__dirp)
{
    errno = ENOSYS;
}
