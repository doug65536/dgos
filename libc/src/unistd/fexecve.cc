#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int fexecve(int fd, char **argv, char **envp)
{
    return syscall3(fd, long(argv), long(envp), SYS_fexecve);
}
