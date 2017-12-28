#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int getgroups(int gidsetsize, gid_t *grouplist)
{
    return syscall2(gidsetsize, long(grouplist), SYS_getgroups);
}
