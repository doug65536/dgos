#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int creat(char const *path, mode_t mode)
{
    return syscall2(long(path), mode, SYS_creat);
}
