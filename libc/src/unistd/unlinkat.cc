#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int unlinkat(int dirfd, char const *path, int flags)
{
    return syscall3(dirfd, long(path), flags, SYS_unlinkat);
}
