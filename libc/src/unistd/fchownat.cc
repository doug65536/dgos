#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int fchownat(int dirfd, const char *path, uid_t uid, gid_t gid, int flags)
{
    return syscall5(dirfd, long(path), uid, gid, flags, SYS_fchownat);
}
