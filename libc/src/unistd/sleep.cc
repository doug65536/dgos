#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

unsigned sleep(unsigned ms)
{
    return syscall1(ms, SYS_sleep);
}
