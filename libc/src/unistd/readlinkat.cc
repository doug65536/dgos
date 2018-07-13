#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

ssize_t readlinkat(int dirfd, char const *restrict path,
                   char *restrict buf, size_t sz)
{
    return syscall4(dirfd, long(path), long(buf), sz, SYS_readlinkat);
}
