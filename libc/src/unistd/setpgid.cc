#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int setpgid(pid_t pid, pid_t pgid)
{
    return syscall2(pid, pgid, SYS_setpgid);
}
