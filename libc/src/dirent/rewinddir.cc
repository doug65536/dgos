#include <dirent.h>
#include <errno.h>

void rewinddir(DIR *__dirp)
{
    errno = ENOSYS;
}
