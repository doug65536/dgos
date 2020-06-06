#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int fexecve(int fd, char **argv, char **envp)
{
    long status = syscall3(fd, uintptr_t(argv), uintptr_t(envp), SYS_fexecve);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
