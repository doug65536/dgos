#include "cpu_aarch64.h"
#include "paging.h"
#include "gdt_sel.h"
#include "screen.h"
#include "cpuid.h"

_section(".smp.data") bool nx_available;
bool gp_available;

void cpu_init()
{
    nx_available = arch_has_no_execute();
    gp_available = arch_has_global_pages();
}
