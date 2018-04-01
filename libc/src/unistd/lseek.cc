#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

off_t lseek(int fd, off_t off, int whence)
{
    return syscall3(long(fd), off, long(whence), SYS_lseek);
}
