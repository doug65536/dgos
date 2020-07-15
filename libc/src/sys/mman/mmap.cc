#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

void *mmap(void *addr, size_t length, int prot,
           int flags, int fd, off_t offset)
{
    void *result = (void*)syscall6(uintptr_t(addr), length, unsigned(prot),
                                   unsigned(flags), fd, offset, SYS_mmap);

    if ((unsigned long)result > 256)
        return result;

    errno = errno_t(uintptr_t(result));

    return (void*)-1;   // MAP_FAILED
}
