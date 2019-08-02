#include "syscall/sys_render.h"
#include <errno.h>
#include "framebuffer.h"

int sys_render_batch(void *batch, size_t sz)
{
    return -int(errno_t::ENOSYS);
}

