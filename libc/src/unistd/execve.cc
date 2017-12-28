#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int execve(const char *path, char **argv, char **envp)
{
    return syscall3(long(path), long(argv), long(envp), SYS_execve);
}
