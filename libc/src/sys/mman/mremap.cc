#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

void *mremap(void *old_address, size_t old_size, size_t new_size,
             int flags, void *new_address)
{
    void *result = (void*)syscall5(uintptr_t(old_address),
                                   old_size, new_size,
                                   unsigned(flags), uintptr_t(new_address),
                                   SYS_mremap);

    if ((unsigned long)result > 256)
        return result;

    errno = errno_t(long(result));

    return (void*)-1;
}
