#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>
#include <errno.h>

int madvise(void *__addr, size_t __len, int __advice)
{
    int result = syscall3(scp_t(__addr), __len, unsigned(__advice),
                          SYS_madvise);

    if (unlikely(result < 0)) {
        errno = -result;
        return -1;
    }

    return result;
}
