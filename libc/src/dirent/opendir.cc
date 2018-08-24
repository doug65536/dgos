#include <dirent.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include "bits/dirent.h"

DIR *opendir(char const *__pathname)
{
    errno = ENOSYS;
    return nullptr;
}
