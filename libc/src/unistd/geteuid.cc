#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

uid_t geteuid(void)
{
    return syscall0(SYS_geteuid);
}
