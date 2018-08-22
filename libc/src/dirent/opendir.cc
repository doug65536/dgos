#include <dirent.h>
#include <errno.h>

DIR *opendir(char const *__pathname)
{
    errno = ENOSYS;
    return nullptr;
}
