#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int truncate(char const *path, off_t sz)
{
    return syscall2(long(path), sz, SYS_truncate);
}
