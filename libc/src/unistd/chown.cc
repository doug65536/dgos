#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int chown(char const *path, uid_t uid, gid_t gid)
{
    return syscall3(long(path), long(uid), long(gid), SYS_chown);
}
