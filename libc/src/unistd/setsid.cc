#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

pid_t setsid(void)
{
    return syscall0(SYS_setsid);
}
