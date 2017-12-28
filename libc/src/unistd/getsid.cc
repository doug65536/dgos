#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

pid_t getsid(pid_t pid)
{
    return syscall1(pid, SYS_getsid);
}
