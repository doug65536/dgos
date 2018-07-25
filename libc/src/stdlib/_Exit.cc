#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

void _Exit(int exitcode)
{
    syscall1(exitcode, SYS_exit);
    __builtin_unreachable();
}
