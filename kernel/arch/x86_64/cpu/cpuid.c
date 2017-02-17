#include "cpuid.h"
#include "string.h"

// max_leaf[0] holds max supported leaf
// max_leaf[1] holds max supported extended leaf
static uint32_t max_leaf[2];

int cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx)
{
    // Automatically check for support for the leaf
    if ((eax & 0x7FFFFFFF) != 0) {
        // Try to use cached information
        uint32_t i = eax >> 31;
        if (max_leaf[i] != 0) {
            output->eax = max_leaf[i];
        } else {
            cpuid(output, eax & 0x80000000, 0);
            max_leaf[i] = output->eax;
        }

        if (output->eax < eax) {
            memset(output, 0, sizeof(*output));
            return 0;
        }
    }

    __asm__ __volatile__ (
        "cpuid"
        : "=a" (output->eax), "=c" (output->ecx),
          "=d" (output->edx), "=b" (output->ebx)
        : "a" (eax), "c" (ecx)
    );

    return 1;
}

int cpuid_eax_bit(int bit, uint32_t eax, uint32_t ecx)
{
    cpuid_t cpuinfo;
    return cpuid(&cpuinfo, eax, ecx) &
            (cpuinfo.eax >> bit) & 1;
}

int cpuid_ebx_bit(int bit, uint32_t eax, uint32_t ecx)
{
    cpuid_t cpuinfo;
    return cpuid(&cpuinfo, eax, ecx) &
            (cpuinfo.ebx >> bit) & 1;
}

int cpuid_ecx_bit(int bit, uint32_t eax, uint32_t ecx)
{
    cpuid_t cpuinfo;
    return cpuid(&cpuinfo, eax, ecx) &
            (cpuinfo.ecx >> bit) & 1;
}

int cpuid_edx_bit(int bit, uint32_t eax, uint32_t ecx)
{
    cpuid_t cpuinfo;
    return cpuid(&cpuinfo, eax, ecx) &
            (cpuinfo.edx >> bit) & 1;
}
