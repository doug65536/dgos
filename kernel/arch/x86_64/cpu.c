
#include "cpu.h"
#include "mmu.h"
#include "gdt.h"

int init_cpu(void)
{
    init_gdt();
    mmu_init();

    return 0;
}
