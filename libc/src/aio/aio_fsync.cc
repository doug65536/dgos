#include <aio.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>

int aio_fsync(int, aiocb *cb)
{
    errno = ENOSYS;
    return -1;
}

