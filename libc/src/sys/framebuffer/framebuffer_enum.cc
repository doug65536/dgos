#include <sys/framebuffer.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>

long framebuffer_enum(size_t index, size_t count, fb_info_t *result_ptr)
{
    long result = syscall3(scp_t(index), scp_t(count), scp_t(result_ptr),
                           SYS_framebuffer_enum);

    if (unlikely(result < 0)) {
        errno = -result;
        return -1;
    }

    return 1;
}
