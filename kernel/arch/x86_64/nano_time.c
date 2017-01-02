#include "time.h"
#include "cpu/control_regs.h"

uint64_t nano_time(void)
{
    return cpu_rdtsc();
}
