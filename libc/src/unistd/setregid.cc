#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int setregid(gid_t rgid, gid_t egid)
{
    return syscall2(rgid, egid, SYS_setregid);
}
