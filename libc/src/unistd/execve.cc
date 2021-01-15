#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int execve(char const *path, char **argv, char **envp)
{
    long status = syscall3(uintptr_t(path), uintptr_t(argv),
                           uintptr_t(envp), SYS_execve);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
