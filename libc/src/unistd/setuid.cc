#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int setuid(uid_t uid)
{
    return syscall1(uid, SYS_setuid);
}
