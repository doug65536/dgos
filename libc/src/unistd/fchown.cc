#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int fchown(int fd, uid_t uid, gid_t gid)
{
    return syscall3(fd, uid, gid, SYS_fchown);
}
