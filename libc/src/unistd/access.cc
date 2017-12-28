#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int access(const char *path, int mode)
{
    return syscall2(long(path), long(mode), SYS_access);
}
