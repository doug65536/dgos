#include <aio.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>

int aio_suspend(const aiocb * const *cbs, int, const timespec *timeout)
{
    errno = ENOSYS;
    return -1;
}
