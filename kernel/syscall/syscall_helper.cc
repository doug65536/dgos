#include "syscall_helper.h"
#include "mm.h"

bool verify_accessible(void const *addr, size_t len, bool writable)
{
    return mpresent(uintptr_t(addr), len);
}
