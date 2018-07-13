#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

ssize_t readlink(char const *restrict path, char *restrict buf, size_t sz)
{
    return syscall3(long(path), long(buf), sz, SYS_readlink);
}
