#include <aio.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>

ssize_t aio_return(aiocb *cb)
{
    errno = ENOSYS;
    return -1;
}
