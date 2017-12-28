#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

uid_t getuid(void)
{
    return syscall0(SYS_getuid);
}
