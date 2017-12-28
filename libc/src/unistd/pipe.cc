#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int pipe(int *fds)
{
    return syscall1(long(fds), SYS_pipe);
}
