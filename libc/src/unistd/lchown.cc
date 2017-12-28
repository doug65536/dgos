#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int lchown(char const *path, uid_t uid, gid_t gid)
{
    return syscall3(long(path), uid, gid, SYS_lchown);
}
