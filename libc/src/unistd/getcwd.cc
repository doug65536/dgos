#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

char *getcwd(char *buf, size_t sz)
{
    return (char*)syscall2(long(buf), sz, SYS_getcwd);
}
