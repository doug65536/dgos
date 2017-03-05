#include "cpuid.h"
#include "string.h"
#include "likely.h"

// max_leaf[0] holds max supported leaf
// max_leaf[1] holds max supported extended leaf
static uint32_t max_leaf[2];

// Cache the most commonly needed leaves
static int cpuid_cache_ready;
static cpuid_t cpuid_00000001;
static cpuid_t cpuid_00000007;
static cpuid_t cpuid_80000001;

int cpuid_nocache(cpuid_t *output, uint32_t eax, uint32_t ecx)
{
    // Automatically check for support for the leaf
    if ((eax & 0x7FFFFFFF) != 0) {
        // Try to use cached information
        uint32_t i = eax >> 31;
        if (max_leaf[i] != 0) {
            output->eax = max_leaf[i];
        } else {
            cpuid_nocache(output, eax & 0x80000000, 0);
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

int cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx)
{
    if (unlikely(!cpuid_cache_ready)) {
        cpuid_nocache(&cpuid_00000001, 0x00000001, 0);
        cpuid_nocache(&cpuid_00000007, 0x00000007, 0);
        cpuid_nocache(&cpuid_80000001, 0x80000001, 0);
        cpuid_cache_ready = 1;
    }

    switch (eax | ((uint64_t)ecx << 32)) {
    case 0x00000001:
        *output = cpuid_00000001;
        return max_leaf[0] >= 0x00000001;

    case 0x00000007:
        *output = cpuid_00000007;
        return max_leaf[0] >= 0x00000007;

    case 0x80000001:
        *output = cpuid_80000001;
        return max_leaf[1] >= 0x80000001;
    }

    return cpuid_nocache(output, eax, ecx);
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
