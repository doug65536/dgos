#include "sys_mem.h"
#include "mm.h"
#include "thread.h"

static bool validate_user_mmop(
        void const *addr, size_t len, int prot, int flags)
{
    intptr_t naddr = (intptr_t)addr;

    if (naddr < 0 ||
            len > INTPTR_MAX ||
            uintptr_t(INTPTR_MAX) - len < uintptr_t(naddr) ||
            (prot & (PROT_READ | PROT_WRITE | PROT_EXEC)) != prot ||
            (flags & MAP_USER_MASK) != flags) {
        return false;
    }

    return true;
}

void *sys_mmap(void *addr, size_t len, int prot,
               int flags, int fd, off_t offset)
{
    if (unlikely(!validate_user_mmop(addr, len, prot, flags)))
        return (void*)errno_t::EINVAL;

    void *result = mmap(addr, len, prot, flags | MAP_USER, fd, offset);

    if (likely(result != MAP_FAILED))
        return result;

    return (void*)errno_t::ENOMEM;
}

int sys_mprotect(void *addr, size_t len, int prot)
{
    if (!validate_user_mmop(addr, len, prot, 0))
        return -int(errno_t::EINVAL);

    return sys_mprotect(addr, len, prot);
}

int sys_munmap(void *addr, size_t size)
{
    if (!validate_user_mmop(addr, size, 0, 0))
        return -int(errno_t::EINVAL);

    return munmap(addr, size);
}

void *sys_mremap(void *old_address, size_t old_size,
                 size_t new_size, int flags, void *new_address)
{
    if (!validate_user_mmop(old_address, old_size, 0, flags))
        return MAP_FAILED;

    if (flags & MAP_FIXED && !validate_user_mmop(
                new_address, new_size, 0, flags))
        return MAP_FAILED;

    errno_t err = errno_t::EINVAL;

    void *result = mremap(old_address, old_size, new_size, flags,
                          new_address, &err);

    if (likely(result != MAP_FAILED))
        return result;

    return (void*)err;
}

int sys_madvise(void *addr, size_t len, int advice)
{
    if (!validate_user_mmop(addr, len, 0, 0))
        return -int(errno_t::EINVAL);

    return madvise(addr, len, advice);
}

int sys_msync(void const *addr, size_t len, int flags)
{
    if (!validate_user_mmop(addr, len, 0, flags))
        return -int(errno_t::EINVAL);

    return msync(addr, len, flags);
}

int sys_mlock(void const *, size_t)
{
    return -int(errno_t::ENOSYS);
}

int sys_munlock(void const *, size_t)
{
    return -int(errno_t::ENOSYS);
}

int sys_mlockall(int)
{
    return -int(errno_t::ENOSYS);
}

int sys_munlockall()
{
    return -int(errno_t::ENOSYS);
}

int clone(int flags, void *child_stack,
          int *ptid, unsigned long newtls,
          int *ctid)
{
    return -int(errno_t::ENOSYS);
}
