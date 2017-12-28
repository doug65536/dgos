#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

unsigned alarm(unsigned seconds)
{
    return syscall1(long(seconds), SYS_alarm);
}
