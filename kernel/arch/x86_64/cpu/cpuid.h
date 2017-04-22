#pragma once

#include "types.h"

struct cpuid_t {
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
};

extern "C" void cpuid_init(void);

// Force use of CPUID instruction (for getting APIC ID)
// Returns true if the CPU supports that leaf
int cpuid_nocache(cpuid_t *output, uint32_t eax, uint32_t ecx);

// Allow cached data
// Returns true if the CPU supports that leaf
int cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx);

int cpuid_eax_bit(int bit, uint32_t eax, uint32_t ecx);
int cpuid_ebx_bit(int bit, uint32_t eax, uint32_t ecx);
int cpuid_ecx_bit(int bit, uint32_t eax, uint32_t ecx);
int cpuid_edx_bit(int bit, uint32_t eax, uint32_t ecx);

#define CPUID_INFO_FEATURES     1
#define CPUID_INFO_XSAVE        0xD

struct cpuid_cache_t {
    bool has_nx      :1;
    bool has_sse3    :1;
    bool has_mwait   :1;
    bool has_ssse3   :1;
    bool has_fma     :1;
    bool has_pge     :1;
    bool has_pcid    :1;
    bool has_invpcid :1;
    bool has_sse4_1  :1;
    bool has_sse4_2  :1;
    bool has_x2apic  :1;
    bool has_aes     :1;
    bool has_xsave   :1;
    bool has_avx     :1;
    bool has_rdrand  :1;
    bool has_smep    :1;
    bool has_de      :1;
    bool has_inrdtsc :1;
};

extern cpuid_cache_t cpuid_cache;

// No eXecute bit in page tables
__const
static inline bool cpuid_has_nx(void)      { return cpuid_cache.has_nx;      }

// SSE instructions
__const
static inline bool cpuid_has_sse3(void)    { return cpuid_cache.has_sse3;    }

// MONITOR/MWAIT instructions
__const
static inline bool cpuid_has_mwait(void)   { return cpuid_cache.has_mwait;   }

// SSE3 instructions
__const
static inline bool cpuid_has_ssse3(void)   { return cpuid_cache.has_ssse3;   }

// Fused Multiply Add instructions
__const
static inline bool cpuid_has_fma(void)     { return cpuid_cache.has_fma;     }

// Page Global Enable capability
__const
static inline bool cpuid_has_pge(void)     { return cpuid_cache.has_pge;     }

// Process Context Identifiers
__const
static inline bool cpuid_has_pcid(void)    { return cpuid_cache.has_pcid;    }

// Invalidate Process Context Identifier instruction
__const
static inline bool cpuid_has_invpcid(void) { return cpuid_cache.has_invpcid; }

// SSE4.1 instructions
__const
static inline bool cpuid_has_sse4_1(void)  { return cpuid_cache.has_sse4_1;  }

// SSE4.2 instructions
__const
static inline bool cpuid_has_sse4_2(void)  { return cpuid_cache.has_sse4_2;  }

// x2APIC present
__const
static inline bool cpuid_has_x2apic(void)  { return cpuid_cache.has_x2apic;  }

// Advanced Encryption Standard instructions
__const
static inline bool cpuid_has_aes(void)     { return cpuid_cache.has_aes;     }

// eXtented Save instructions
__const
static inline bool cpuid_has_xsave(void)   { return cpuid_cache.has_xsave;   }

// Advanced Vector Extensions instructions
__const
static inline bool cpuid_has_avx(void)     { return cpuid_cache.has_avx;     }

// ReaD RAND instruction
__const
static inline bool cpuid_has_rdrand(void)  { return cpuid_cache.has_rdrand;  }

// Supervisor Mode Execution Prevention
__const
static inline bool cpuid_has_smep(void)    { return cpuid_cache.has_smep;    }

// Debugging Extensions
__const
static inline bool cpuid_has_de(void)      { return cpuid_cache.has_de;      }

// INvariant ReaD TimeStamp Counter
__const
static inline bool cpuid_has_inrdtsc(void) { return cpuid_cache.has_inrdtsc; }
