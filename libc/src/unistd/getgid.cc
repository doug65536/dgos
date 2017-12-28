#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

gid_t getgid()
{
    return syscall0(SYS_getgid);
}
