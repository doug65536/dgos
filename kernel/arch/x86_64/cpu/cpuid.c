#include "cpuid.h"
#include "string.h"
#include "likely.h"

// max_leaf[0] holds max supported leaf
// max_leaf[1] holds max supported extended leaf
static uint32_t max_leaf[2];

cpuid_cache_t cpuid_cache;

void cpuid_init(void)
{
    cpuid_cache.has_nx      = cpuid_edx_bit(20, 0x80000001, 0);
    cpuid_cache.has_sse3    = cpuid_ecx_bit(0, 1, 0);
    cpuid_cache.has_mwait   = cpuid_ecx_bit(3, 1, 0);
    cpuid_cache.has_ssse3   = cpuid_ecx_bit(9, 1, 0);
    cpuid_cache.has_fma     = cpuid_ecx_bit(12, 1, 0);
    cpuid_cache.has_pge     = cpuid_edx_bit(13, 1, 0);
    cpuid_cache.has_pcid    = cpuid_ecx_bit(17, 1, 0);
    cpuid_cache.has_invpcid = cpuid_ebx_bit(10, 7, 0);
    cpuid_cache.has_sse4_1  = cpuid_ecx_bit(19, 1, 0);
    cpuid_cache.has_sse4_2  = cpuid_ecx_bit(20, 1, 0);
    cpuid_cache.has_x2apic  = cpuid_ecx_bit(21, 1, 0);
    cpuid_cache.has_aes     = cpuid_ecx_bit(25, 1, 0);
    cpuid_cache.has_xsave   = cpuid_ecx_bit(26, 1, 0);
    cpuid_cache.has_avx     = cpuid_ecx_bit(28, 1, 0);
    cpuid_cache.has_rdrand  = cpuid_ecx_bit(30, 1, 0);
    cpuid_cache.has_smep    = cpuid_ebx_bit(7, 7, 0);
    cpuid_cache.has_de      = cpuid_edx_bit(2, 1, 0);
}

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
        : "=a" (output->eax), "=c" (output->ecx)
        , "=d" (output->edx), "=b" (output->ebx)
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
