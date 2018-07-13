#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int linkat(int targetdirfd, char const *target,
           int linkdirfd, char const *link, int flags)
{
    return syscall5(targetdirfd, long(target), linkdirfd,
                    long(link), flags, SYS_linkat);
}
