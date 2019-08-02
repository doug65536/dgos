#include "mm.h"

inval_user_pf::inval_user_pf(uintptr_t address, bool insn, bool write)
    : address(address)
    , insn(insn)
    , write(write)
{
}

char const *inval_user_pf::what() const noexcept
{
    return "User space page fault";
}
