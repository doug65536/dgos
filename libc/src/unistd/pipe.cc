#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

// Create two file descriptors, the first for the read end of the pipe,
// the second for the write end of the pipe
int pipe(int *fds)
{
    long status = syscall1(uintptr_t(fds), SYS_pipe);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
