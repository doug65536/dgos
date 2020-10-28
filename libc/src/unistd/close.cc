#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>
#include <sys/likely.h>

int close(int fd)
{
    long status = syscall1(fd, SYS_close);

    if (unlikely(status < 0)) {
        errno = -status;    
        return -1;
    }
    
    return status;
}
