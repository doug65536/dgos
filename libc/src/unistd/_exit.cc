#include <unistd.h>
#include "sys/syscall.h"
#include "sys/syscall_num.h"

extern "C" void exit(int exitcode)
{
    syscall1(SYS_exit, exitcode);
    __builtin_unreachable();
}
