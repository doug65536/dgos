#define CPUID_CC
#include "cpuid.h"
#include "string.h"
#include "likely.h"
#include "control_regs_constants.h"

// max_leaf[0] holds max supported leaf
// max_leaf[1] holds max supported extended leaf
static uint32_t max_leaf[2];

cpuid_cache_t cpuid_cache;
int cpuid_nx_mask;

void cpuid_init(void)
{
    cpuid_t info;

    if (cpuid(&info, CPUID_HIGHESTFUNC, 0)) {
        if (!memcmp(&info.ebx, "AuthenticAMD", 12))
            cpuid_cache.is_amd = 1;
        else if (!memcmp(&info.ebx, "GenuineIntel", 12))
            cpuid_cache.is_intel = 1;
    }

    if (cpuid(&info, CPUID_INFO_FEATURES, 0)) {
        cpuid_cache.has_de      = info.edx & (1U << 2);
        cpuid_cache.has_pge     = info.edx & (1U << 13);
        cpuid_cache.has_sysenter= info.edx & (1U << 11);

        cpuid_cache.has_sse3    = info.ecx & (1U << 0);
        cpuid_cache.has_mwait   = info.ecx & (1U << 3);
        cpuid_cache.has_ssse3   = info.ecx & (1U << 9);
        cpuid_cache.has_fma     = info.ecx & (1U << 12);
        cpuid_cache.has_pcid    = info.ecx & (1U << 17);
        cpuid_cache.has_sse4_1  = info.ecx & (1U << 19);
        cpuid_cache.has_sse4_2  = info.ecx & (1U << 20);
        cpuid_cache.has_x2apic  = info.ecx & (1U << 21);
        cpuid_cache.has_aes     = info.ecx & (1U << 25);
        cpuid_cache.has_xsave   = info.ecx & (1U << 26);
        cpuid_cache.has_avx     = info.ecx & (1U << 28);
        cpuid_cache.has_rdrand  = info.ecx & (1U << 30);
    }

    if (cpuid(&info, CPUID_EXTINFO_FEATURES, 0)) {
        cpuid_cache.has_2mpage  = info.edx & (1U << 3);
        cpuid_cache.has_1gpage  = info.edx & (1U << 26);
        cpuid_cache.has_nx      = info.edx & (1U << 20);
    }

    if (cpuid(&info, CPUID_INFO_EXT_FEATURES, 0)) {
        cpuid_cache.has_fsgsbase= info.ebx & (1U << 0);
        cpuid_cache.has_umip    = info.ecx & (1U << 2);
        cpuid_cache.has_smep    = info.ebx & (1U << 7);
        cpuid_cache.has_erms    = info.ebx & (1U << 9);
        cpuid_cache.has_invpcid = info.ebx & (1U << 10);
        cpuid_cache.has_avx512f = info.ebx & (1U << 16);
        cpuid_cache.has_smap    = info.ebx & (1U << 20);
    }

    if (cpuid(&info, CPUID_APM, 0)) {
        cpuid_cache.has_inrdtsc = info.edx & (1U << 8);
    }

    cpuid_nx_mask = -!!cpuid_cache.has_nx;

    if (cpuid(&info, CPUID_MONITOR, 0)) {
        cpuid_cache.min_monitor_line = uint16_t(info.eax);
        cpuid_cache.max_monitor_line = uint16_t(info.ebx);
    }

    if (cpuid(&info, CPUID_ADDRSIZES, 0)) {
        cpuid_cache.laddr_bits = info.eax & 0xFF;
        cpuid_cache.paddr_bits = (info.eax >> 8) & 0xFF;
    } else {
        // Make reasonable guess
        cpuid_cache.laddr_bits = 48;
        cpuid_cache.paddr_bits = 52;
    }
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

uint32_t cpuid_eax(uint32_t eax, uint32_t ecx)
{
    cpuid_t cpuinfo;
    return -cpuid(&cpuinfo, eax, ecx) & cpuinfo.eax;
}

uint32_t cpuid_ebx(uint32_t eax, uint32_t ecx)
{
    cpuid_t cpuinfo;
    return -cpuid(&cpuinfo, eax, ecx) & cpuinfo.ebx;
}

uint32_t cpuid_ecx(uint32_t eax, uint32_t ecx)
{
    cpuid_t cpuinfo;
    return -cpuid(&cpuinfo, eax, ecx) & cpuinfo.ecx;
}

uint32_t cpuid_edx(uint32_t eax, uint32_t ecx)
{
    cpuid_t cpuinfo;
    return -cpuid(&cpuinfo, eax, ecx) & cpuinfo.edx;
}

bool cpuid_eax_bit(int bit, uint32_t eax, uint32_t ecx)
{
    return (cpuid_eax(eax, ecx) >> bit) & 1;
}

bool cpuid_ebx_bit(int bit, uint32_t eax, uint32_t ecx)
{
    return (cpuid_ebx(eax, ecx) >> bit) & 1;
}

bool cpuid_ecx_bit(int bit, uint32_t eax, uint32_t ecx)
{
    return (cpuid_ecx(eax, ecx) >> bit) & 1;
}

bool cpuid_edx_bit(int bit, uint32_t eax, uint32_t ecx)
{
    return (cpuid_edx(eax, ecx) >> bit) & 1;
}
