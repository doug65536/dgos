#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int link(char const *target, char const *link)
{
    return syscall2(long(target), long(link), SYS_link);
}
