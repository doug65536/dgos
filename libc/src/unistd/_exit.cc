#include <unistd.h>
#include "sys/syscall.h"
#include "sys/syscall_num.h"

extern "C" void exit(int exitcode)
{
    syscall1(unsigned(exitcode), SYS_exit);
    __builtin_unreachable();
}
