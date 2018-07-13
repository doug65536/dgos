#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int faccessat(int dirfd, char const *path, int mode, int flags)
{
    return syscall4(dirfd, long(path), mode, flags, SYS_faccessat);
}
