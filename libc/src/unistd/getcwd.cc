#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

char *getcwd(char *buf, size_t sz)
{
    long status = syscall2(long(buf), sz, SYS_getcwd);

    if (status >= 0)
        return buf;

    errno = -status;

    return nullptr;
}
