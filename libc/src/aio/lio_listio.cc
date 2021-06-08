#include <aio.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>
#include <sys/likely.h>

int lio_listio(int mode, aiocb * const *list, int count, sigevent *sig)
{
    if (unlikely((mode != LIO_WAIT && mode != LIO_NOWAIT) ||
                 count < 0 || (count && !list) || count > AIO_LISTIO_MAX)) {
        errno = EINVAL;
        return -1;
    }

    if
}
