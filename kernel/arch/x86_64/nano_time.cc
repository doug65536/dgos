#include "nano_time.h"
#include "time.h"
#include "cpu/control_regs.h"

uint64_t rdtsc_freq;
uint64_t clk_to_ns_numer;
uint64_t clk_to_ns_denom;

uint64_t nano_time(void)
{
    return cpu_rdtsc();
}

uint64_t nano_time_ns(uint64_t a, uint64_t b)
{
    return ((b - a) * clk_to_ns_numer) / clk_to_ns_denom;
}

uint64_t nano_time_add(uint64_t after, uint64_t ns)
{
    return after + (ns * clk_to_ns_denom) / clk_to_ns_numer;
}
