#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int rmdir(char const *path)
{
    return syscall1(long(path), SYS_rmdir);
}
