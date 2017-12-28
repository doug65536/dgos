#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int ftruncate(int fd, off_t sz)
{
    return syscall2(fd, sz, SYS_ftruncate);
}
