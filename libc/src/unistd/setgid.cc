#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int setgid(gid_t gid)
{
    return syscall1(gid, SYS_setgid);
}
