#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

char *getcwd(char *buf, size_t sz)
{
    long status = syscall2(uintptr_t(buf), sz, SYS_getcwd);

    if (likely(status >= 0))
        return buf;

    errno = -status;

    return nullptr;
}
