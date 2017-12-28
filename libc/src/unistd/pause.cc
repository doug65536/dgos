#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int pause()
{
    return syscall0(SYS_pause);
}
