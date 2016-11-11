#include "mmu.h"
#include "msr.h"
#include "printk.h"

physmem_range_t *phys_mem_map;

void mmu_init(void)
{
    for (physmem_range_t *mem = phys_mem_map;
         mem->valid; ++mem) {
        printk("Memory: addr=%lx size=%lx type=%x\n",
               mem->base, mem->size, mem->type);
    }
}

void mmu_set_fsgsbase(void *fs_base, void *gs_base)
{
    msr_set(MSR_FSBASE, (uint64_t)fs_base);
    msr_set(MSR_GSBASE, (uint64_t)gs_base);
}
