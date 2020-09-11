#pragma once

#include "types.h"

// In this order to allow trivial memcmp for vendor signature checks
struct cpuid_t {
    uint32_t eax;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
};

extern "C" void _generic_target cpuid_init();

// Force use of CPUID instruction (for getting APIC ID)
// Returns true if the CPU supports that leaf
_generic_target int cpuid_nocache(
        cpuid_t *output, uint32_t eax, uint32_t ecx);

// Allow cached data
// Returns true if the CPU supports that leaf
_generic_target int cpuid(
        cpuid_t *output, uint32_t eax, uint32_t ecx);

_generic_target uint32_t cpuid_eax(uint32_t eax, uint32_t ecx);
_generic_target uint32_t cpuid_ebx(uint32_t eax, uint32_t ecx);
_generic_target uint32_t cpuid_ecx(uint32_t eax, uint32_t ecx);
_generic_target uint32_t cpuid_edx(uint32_t eax, uint32_t ecx);

_generic_target bool cpuid_eax_bit(int bit, uint32_t eax, uint32_t ecx);
_generic_target bool cpuid_ebx_bit(int bit, uint32_t eax, uint32_t ecx);
_generic_target bool cpuid_ecx_bit(int bit, uint32_t eax, uint32_t ecx);
_generic_target bool cpuid_edx_bit(int bit, uint32_t eax, uint32_t ecx);

enum struct hv_type_t : uint8_t {
    NONE,
    KVM,
    TCG,
    UNKNOWN
};

struct cpuid_cache_t {
    unsigned family     :12;
    unsigned model      :12;

    bool is_amd         :1;
    bool is_intel       :1;

    bool has_de         :1;
    bool has_pge        :1;
    bool has_sysenter   :1;
    bool has_pat        :1;
    bool has_self_snoop :1;
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
    bool is_hypervisor  :1;
    bool has_xsaves     :1;
    bool has_xsaveopt   :1;
    bool has_xsavec     :1;
    bool has_2mpage     :1;
    bool has_1gpage     :1;
    bool has_nx         :1;
    bool has_perfctr    :1;
    bool has_fsgsbase   :1;
    bool has_umip       :1;
    bool has_smep       :1;
    bool has_erms       :1;
    bool has_invpcid    :1;
    bool has_avx512f    :1;
    bool has_smap       :1;
    bool has_inrdtsc    :1;

    // ibrs includes ibpb
    bool has_ibrs       :1;
    bool has_stibp      :1;
    bool has_ssbd       :1;
    bool has_l1df       :1;
    bool has_md_clear   :1;

    bool bug_meltdown   :1;

    hv_type_t hv_type;

    uint16_t min_monitor_line;
    uint16_t max_monitor_line;
    uint8_t paddr_bits;
    uint8_t laddr_bits;

    char brand[48];
};

#ifdef CPUID_CC
#define CPUID_CONST_INLINE \
    extern "C" _const
#else
#define CPUID_CONST_INLINE \
    static _always_inline _generic_target _const
#endif

extern cpuid_cache_t cpuid_cache;
extern int cpuid_nx_mask;

CPUID_CONST_INLINE unsigned cpuid_family()
{
    return cpuid_cache.family;
}

CPUID_CONST_INLINE unsigned cpuid_model()
{
    return cpuid_cache.model;
}

// CPU is AuthenticAMD
CPUID_CONST_INLINE bool cpuid_is_amd()
{
    return cpuid_cache.is_amd;
}

// CPU is GenuineIntel
CPUID_CONST_INLINE bool cpuid_is_intel()
{
    return cpuid_cache.is_intel;
}

// Running under a hypervisor
CPUID_CONST_INLINE bool cpuid_is_hypervisor()
{
    return cpuid_cache.is_hypervisor;
}

// No eXecute bit in page tables
CPUID_CONST_INLINE bool cpuid_has_nx()
{
    return cpuid_cache.has_nx;
}

// Performance counter MSRs
CPUID_CONST_INLINE bool cpuid_has_perf_ctr()
{
    return cpuid_cache.has_perfctr;
}

// SSE instructions
CPUID_CONST_INLINE bool cpuid_has_sse3()
{
    return cpuid_cache.has_sse3;
}

// MONITOR/MWAIT instructions
CPUID_CONST_INLINE bool cpuid_has_mwait()
{
    return cpuid_cache.has_mwait;
}

// SSE3 instructions
CPUID_CONST_INLINE bool cpuid_has_ssse3()
{
    return cpuid_cache.has_ssse3;
}

// Fused Multiply Add instructions
CPUID_CONST_INLINE bool cpuid_has_fma()
{
    return cpuid_cache.has_fma;
}

// Page Global Enable capability
CPUID_CONST_INLINE bool cpuid_has_pge()
{
    return cpuid_cache.has_pge;
}

// Process Context Identifiers
CPUID_CONST_INLINE bool cpuid_has_pcid()
{
    return cpuid_cache.has_pcid;
}

// Invalidate Process Context Identifier instruction
CPUID_CONST_INLINE bool cpuid_has_invpcid()
{
    return cpuid_cache.has_invpcid;
}

// SSE4.1 instructions
CPUID_CONST_INLINE bool cpuid_has_sse4_1()
{
    return cpuid_cache.has_sse4_1;
}

// SSE4.2 instructions
CPUID_CONST_INLINE bool cpuid_has_sse4_2()
{
    return cpuid_cache.has_sse4_2;
}

// x2APIC present
CPUID_CONST_INLINE bool cpuid_has_x2apic()
{
    return cpuid_cache.has_x2apic;
}

// Advanced Encryption Standard instructions
CPUID_CONST_INLINE bool cpuid_has_aes()
{
    return cpuid_cache.has_aes;
}

// eXtented Save instructions
CPUID_CONST_INLINE bool cpuid_has_xsave()
{
    return cpuid_cache.has_xsave;
}

// xsaves instruction (supervisor)
CPUID_CONST_INLINE bool cpuid_has_xsaves()
{
    return cpuid_cache.has_xsaves;
}

// xsaveopt instruction (optimized)
CPUID_CONST_INLINE bool cpuid_has_xsaveopt()
{
    return cpuid_cache.has_xsave;
}

// xsavec instruction (compact)
CPUID_CONST_INLINE bool cpuid_has_xsavec()
{
    return cpuid_cache.has_xsavec;
}

// Advanced Vector Extensions instructions
CPUID_CONST_INLINE bool cpuid_has_avx()
{
    return cpuid_cache.has_avx;
}

// ReaD RAND instruction
CPUID_CONST_INLINE bool cpuid_has_rdrand()
{
    return cpuid_cache.has_rdrand;
}

// Supervisor Mode Execution Prevention
CPUID_CONST_INLINE bool cpuid_has_smep()
{
    return cpuid_cache.has_smep;
}

// Supervisor Mode Access Prevention
CPUID_CONST_INLINE bool cpuid_has_smap()
{
    return cpuid_cache.has_smap;
}

// Debugging Extensions
CPUID_CONST_INLINE bool cpuid_has_de()
{
    return cpuid_cache.has_de;
}

// INvariant ReaD TimeStamp Counter
CPUID_CONST_INLINE bool cpuid_has_inrdtsc()
{
    return cpuid_cache.has_inrdtsc;
}

// Avx-512 Foundation
CPUID_CONST_INLINE bool cpuid_has_avx512f()
{
    return cpuid_cache.has_avx512f;
}

// {RD|WR}{FS|GS}BASE instructions
CPUID_CONST_INLINE bool cpuid_has_fsgsbase()
{
    return cpuid_cache.has_fsgsbase;
}

// SYSENTER/SYSEXIT instructions
CPUID_CONST_INLINE bool cpuid_has_sysenter()
{
    return cpuid_cache.has_sysenter;
}

// Paging attribute table MSR and PTE bits
CPUID_CONST_INLINE bool cpuid_has_pat()
{
    return cpuid_cache.has_pat;
}

// Self snoop: "management of conflicting memory types by performing a
// snoop of its own cache structure for transactions issued to the bus"
CPUID_CONST_INLINE bool cpuid_has_self_snoop()
{
    return cpuid_cache.has_self_snoop;
}

// 2MB pages
CPUID_CONST_INLINE bool cpuid_has_2mpage()
{
    return cpuid_cache.has_2mpage;
}

// 1GB pages
CPUID_CONST_INLINE bool cpuid_has_1gpage()
{
    return cpuid_cache.has_1gpage;
}

// User mode instruction prevention
CPUID_CONST_INLINE bool cpuid_has_umip()
{
    return cpuid_cache.has_umip;
}

// Enhanced rep move string
CPUID_CONST_INLINE bool cpuid_has_erms()
{
    return cpuid_cache.has_erms;
}

// Linear address size in bits
CPUID_CONST_INLINE uint8_t cpuid_laddr_bits()
{
    return cpuid_cache.laddr_bits;
}

// Physical address size in bits
CPUID_CONST_INLINE uint8_t cpuid_paddr_bits()
{
    return cpuid_cache.paddr_bits;
}

CPUID_CONST_INLINE bool cpuid_has_bug_meltdown()
{
    return cpuid_cache.bug_meltdown;
}

CPUID_CONST_INLINE bool cpuid_has_ibpb()
{
    // Same as ibrs
    return cpuid_cache.has_ibrs;
}

CPUID_CONST_INLINE bool cpuid_has_ibrs()
{
    return cpuid_cache.has_ibrs;
}

CPUID_CONST_INLINE bool cpuid_has_stibp()
{
    return cpuid_cache.has_stibp;
}

CPUID_CONST_INLINE bool cpuid_has_ssbd()
{
    return cpuid_cache.has_ssbd;
}

CPUID_CONST_INLINE bool cpuid_has_l1d_flush()
{
    return cpuid_cache.has_l1df;
}

CPUID_CONST_INLINE bool cpuid_has_md_clear()
{
    return cpuid_cache.has_md_clear;
}

CPUID_CONST_INLINE bool cpuid_is_kvm()
{
    return cpuid_cache.hv_type == hv_type_t::KVM;
}

CPUID_CONST_INLINE bool cpuid_is_tcg()
{
    return cpuid_cache.hv_type == hv_type_t::TCG;
}

CPUID_CONST_INLINE char const *cpuid_brand()
{
    return cpuid_cache.brand;
}
