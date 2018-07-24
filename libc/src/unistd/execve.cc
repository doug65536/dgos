#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int execve(char const *path, char **argv, char **envp)
{
    long status = syscall3(long(path), long(argv), long(envp), SYS_execve);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
