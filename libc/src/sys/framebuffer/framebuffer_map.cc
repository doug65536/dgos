#include <sys/framebuffer.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>

long framebuffer_map()
{
    errno = ENOSYS;
    return -1;
}
