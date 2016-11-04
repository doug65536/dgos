
#include "cpu.h"
#include "mmu.h"

int init_cpu(void)
{
    init_mmu();

    return 0;
}
