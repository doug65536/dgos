#include "sys_sys.h"
#include "errno.h"

int sys_reboot(int magic1, int magic2, int cmd, void *arg)
{
    return -int(errno_t::ENOSYS);
}
