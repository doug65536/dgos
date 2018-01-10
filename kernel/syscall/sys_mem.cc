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
        thread_set_error(errno_t::EINVAL);
        return false;
    }

    return true;
}

void *sys_mmap(void *addr, size_t len, int prot,
               int flags, int fd, off_t offset)
{
    if (!validate_user_mmop(addr, len, prot, flags))
        return MAP_FAILED;

    return mmap(addr, len, prot, flags | MAP_USER, fd, offset);
}

int sys_mprotect(void *addr, size_t len, int prot)
{
    if (!validate_user_mmop(addr, len, prot, 0))
        return -1;

    return sys_mprotect(addr, len, prot);
}

int sys_munmap(void *addr, size_t size)
{
    if (!validate_user_mmop(addr, size, 0, 0))
        return -1;

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

    return mremap(old_address, old_size, new_size, flags,
                  new_address);
}

int sys_madvise(void *addr, size_t len, int advice)
{
    if (!validate_user_mmop(addr, len, 0, 0))
        return -1;

    return madvise(addr, len, advice);
}

int sys_msync(const void *addr, size_t len, int flags)
{
    if (!validate_user_mmop(addr, len, 0, flags))
        return -1;

    return msync(addr, len, flags);
}

int sys_mlock(const void *, size_t)
{
    thread_set_error(errno_t::ENOSYS);
    return -1;
}

int sys_munlock(const void *, size_t)
{
    thread_set_error(errno_t::ENOSYS);
    return -1;
}

int sys_mlockall(int)
{
    thread_set_error(errno_t::ENOSYS);
    return -1;
}

int sys_munlockall()
{
    thread_set_error(errno_t::ENOSYS);
    return -1;
}

int clone(int flags, void *child_stack,
          int *ptid, unsigned long newtls,
          int *ctid)
{
    return -1;
}
