#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

void sync(void)
{
    syscall0(SYS_sync);
}
