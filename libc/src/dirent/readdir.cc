#include <dirent.h>
#include <errno.h>

struct dirent *readdir(DIR *__dirp)
{
    errno = ENOSYS;
    return nullptr;
}
