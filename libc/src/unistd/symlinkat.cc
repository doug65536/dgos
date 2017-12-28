#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int symlinkat(char const *target, int dirfd, char const *link)
{
    return syscall3(long(target), dirfd, long(link), SYS_symlinkat);
}
