#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

pid_t getpgid(pid_t gid)
{
    return syscall1(gid, SYS_getpgid);
}
