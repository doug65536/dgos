#pragma once

#include "types.h"

// In this order to allow trivial memcmp for vendor signature checks
struct cpuid_t {
    uint32_t eax;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
};

extern "C" void __generic_target cpuid_init(void);

// Force use of CPUID instruction (for getting APIC ID)
// Returns true if the CPU supports that leaf
__generic_target int cpuid_nocache(
        cpuid_t *output, uint32_t eax, uint32_t ecx);

// Allow cached data
// Returns true if the CPU supports that leaf
__generic_target int cpuid(
        cpuid_t *output, uint32_t eax, uint32_t ecx);

__generic_target uint32_t cpuid_eax(uint32_t eax, uint32_t ecx);
__generic_target uint32_t cpuid_ebx(uint32_t eax, uint32_t ecx);
__generic_target uint32_t cpuid_ecx(uint32_t eax, uint32_t ecx);
__generic_target uint32_t cpuid_edx(uint32_t eax, uint32_t ecx);

__generic_target bool cpuid_eax_bit(int bit, uint32_t eax, uint32_t ecx);
__generic_target bool cpuid_ebx_bit(int bit, uint32_t eax, uint32_t ecx);
__generic_target bool cpuid_ecx_bit(int bit, uint32_t eax, uint32_t ecx);
__generic_target bool cpuid_edx_bit(int bit, uint32_t eax, uint32_t ecx);

struct cpuid_cache_t {
    bool is_amd         :1;
    bool is_intel       :1;

    bool has_de         :1;
    bool has_pge        :1;
    bool has_sysenter   :1;
    bool has_sse3       :1;
    bool has_mwait      :1;
    bool has_ssse3      :1;
    bool has_fma        :1;
    bool has_pcid       :1;
    bool has_sse4_1     :1;
    bool has_sse4_2     :1;
    bool has_x2apic     :1;
    bool has_aes        :1;
    bool has_xsave      :1;
    bool has_avx        :1;
    bool has_rdrand     :1;
    bool has_2mpage     :1;
    bool has_1gpage     :1;
    bool has_nx         :1;
    bool has_fsgsbase   :1;
    bool has_umip       :1;
    bool has_smep       :1;
    bool has_erms       :1;
    bool has_invpcid    :1;
    bool has_avx512f    :1;
    bool has_smap       :1;
    bool has_inrdtsc    :1;

    uint16_t min_monitor_line;
    uint16_t max_monitor_line;
    uint8_t laddr_bits;
    uint8_t paddr_bits;
};

#ifdef CPUID_CC
#define CPUID_CONST_INLINE \
    extern "C" __const
#else
#define CPUID_CONST_INLINE \
    static __always_inline __const
#endif

extern cpuid_cache_t cpuid_cache;
extern int cpuid_nx_mask;

// CPU is AuthenticAMD
CPUID_CONST_INLINE bool cpuid_is_amd(void)
{
    return cpuid_cache.is_amd;
}

// CPU is GenuineIntel
CPUID_CONST_INLINE bool cpuid_is_intel(void)
{
    return cpuid_cache.is_intel;
}

// No eXecute bit in page tables
CPUID_CONST_INLINE bool cpuid_has_nx(void)
{
    return cpuid_cache.has_nx;
}

// SSE instructions
CPUID_CONST_INLINE bool cpuid_has_sse3(void)
{
    return cpuid_cache.has_sse3;
}

// MONITOR/MWAIT instructions
CPUID_CONST_INLINE bool cpuid_has_mwait(void)
{
    return cpuid_cache.has_mwait;
}

// SSE3 instructions
CPUID_CONST_INLINE bool cpuid_has_ssse3(void)
{
    return cpuid_cache.has_ssse3;
}

// Fused Multiply Add instructions
CPUID_CONST_INLINE bool cpuid_has_fma(void)
{
    return cpuid_cache.has_fma;
}

// Page Global Enable capability
CPUID_CONST_INLINE bool cpuid_has_pge(void)
{
    return cpuid_cache.has_pge;
}

// Process Context Identifiers
CPUID_CONST_INLINE bool cpuid_has_pcid(void)
{
    return cpuid_cache.has_pcid;
}

// Invalidate Process Context Identifier instruction
CPUID_CONST_INLINE bool cpuid_has_invpcid(void)
{
    return cpuid_cache.has_invpcid;
}

// SSE4.1 instructions
CPUID_CONST_INLINE bool cpuid_has_sse4_1(void)
{
    return cpuid_cache.has_sse4_1;
}

// SSE4.2 instructions
CPUID_CONST_INLINE bool cpuid_has_sse4_2(void)
{
    return cpuid_cache.has_sse4_2;
}

// x2APIC present
CPUID_CONST_INLINE bool cpuid_has_x2apic(void)
{
    return cpuid_cache.has_x2apic;
}

// Advanced Encryption Standard instructions
CPUID_CONST_INLINE bool cpuid_has_aes(void)
{
    return cpuid_cache.has_aes;
}

// eXtented Save instructions
CPUID_CONST_INLINE bool cpuid_has_xsave(void)
{
    return cpuid_cache.has_xsave;
}

// Advanced Vector Extensions instructions
CPUID_CONST_INLINE bool cpuid_has_avx(void)
{
    return cpuid_cache.has_avx;
}

// ReaD RAND instruction
CPUID_CONST_INLINE bool cpuid_has_rdrand(void)
{
    return cpuid_cache.has_rdrand;
}

// Supervisor Mode Execution Prevention
CPUID_CONST_INLINE bool cpuid_has_smep(void)
{
    return cpuid_cache.has_smep;
}

// Debugging Extensions
CPUID_CONST_INLINE bool cpuid_has_de(void)
{
    return cpuid_cache.has_de;
}

// INvariant ReaD TimeStamp Counter
CPUID_CONST_INLINE bool cpuid_has_inrdtsc(void)
{
    return cpuid_cache.has_inrdtsc;
}

// Avx-512 Foundation
CPUID_CONST_INLINE bool cpuid_has_avx512f(void)
{
    return cpuid_cache.has_avx512f;
}

// {RD|WR}{FS|GS}BASE instructions
CPUID_CONST_INLINE bool cpuid_has_fsgsbase(void)
{
    return cpuid_cache.has_fsgsbase;
}

// SYSENTER/SYSEXIT instructions
CPUID_CONST_INLINE bool cpuid_has_sysenter(void)
{
    return cpuid_cache.has_sysenter;
}

// 2MB pages
CPUID_CONST_INLINE bool cpuid_has_2mpage(void)
{
    return cpuid_cache.has_2mpage;
}

// 1GB pages
CPUID_CONST_INLINE bool cpuid_has_1gpage(void)
{
    return cpuid_cache.has_1gpage;
}

// User mode instruction prevention
CPUID_CONST_INLINE bool cpuid_has_umip(void)
{
    return cpuid_cache.has_umip;
}

// Enhanced rep move string
CPUID_CONST_INLINE bool cpuid_has_erms(void)
{
    return cpuid_cache.has_erms;
}

// Linear address size in bits
CPUID_CONST_INLINE uint8_t cpuid_laddr_bits(void)
{
    return cpuid_cache.laddr_bits;
}

// Physical address size in bits
CPUID_CONST_INLINE uint8_t cpuid_paddr_bits(void)
{
    return cpuid_cache.paddr_bits;
}
