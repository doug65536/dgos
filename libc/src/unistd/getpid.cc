#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

pid_t getpid(void)
{
    return syscall0(SYS_getpid);
}
