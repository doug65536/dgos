#include <unistd.h>
#include <sys/stat.h>

int mkdir(char const *path, mode_t mode)
{
    return mkdirat(AT_FDCWD, path, mode);
}
