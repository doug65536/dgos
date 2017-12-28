#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int setreuid(uid_t ruid, uid_t euid)
{
    return syscall2(ruid, euid, SYS_setresuid);
}
