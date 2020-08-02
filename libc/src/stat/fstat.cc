#include <sys/stat.h>
#include <errno.h>
#include <sys/syscall_num.h>

int fstat(int fd, struct stat *info)
{
    errno = ENOSYS;
    return -1;
}
