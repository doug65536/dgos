#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

int munmap(void *addr, size_t size)
{
    int status = syscall2(long(addr), size, SYS_munmap);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
