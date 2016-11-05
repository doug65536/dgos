
#include "cpu.h"
#include "mmu.h"
#include "gdt.h"

int init_cpu(void)
{
    init_gdt();
    init_mmu();

    return 0;
}
