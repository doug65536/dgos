#include "mmu.h"
#include "msr.h"

int mmu_init(void)
{
    return 0;
}

void mmu_set_fsgsbase(void *fs_base, void *gs_base)
{
    msr_set(MSR_FSBASE, (uint64_t)fs_base);
    msr_set(MSR_GSBASE, (uint64_t)gs_base);
}
