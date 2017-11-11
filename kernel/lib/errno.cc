#include "errno.h"
#include "cpu/asm_constants.h"
#include "assert.h"

C_ASSERT(-int(errno_t::ENOSYS) == SYSCALL_ENOSYS);
