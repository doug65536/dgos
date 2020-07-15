#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>
#include <errno.h>

int mprotect(void *__addr, size_t __len, int __prot)
{
    int result = syscall3(scp_t(__addr), __len, unsigned(__prot),
                          SYS_mprotect);

    if (unlikely(result < 0)) {
        errno = -result;
        return -1;
    }

    return result;
}
