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
    unsigned has_nx      :1;
    unsigned has_sse3    :1;
    unsigned has_mwait   :1;
    unsigned has_ssse3   :1;
    unsigned has_fma     :1;
    unsigned has_pge     :1;
    unsigned has_pcid    :1;
    unsigned has_invpcid :1;
    unsigned has_sse4_1  :1;
    unsigned has_sse4_2  :1;
    unsigned has_x2apic  :1;
    unsigned has_aes     :1;
    unsigned has_xsave   :1;
    unsigned has_avx     :1;
    unsigned has_rdrand  :1;
    unsigned has_smep    :1;
    unsigned has_de      :1;
};

extern cpuid_cache_t cpuid_cache;

__const
static inline int cpuid_has_nx(void)      { return cpuid_cache.has_nx;      }

__const
static inline int cpuid_has_sse3(void)    { return cpuid_cache.has_sse3;    }

__const
static inline int cpuid_has_mwait(void)   { return cpuid_cache.has_mwait;   }

__const
static inline int cpuid_has_ssse3(void)   { return cpuid_cache.has_ssse3;   }

__const
static inline int cpuid_has_fma(void)     { return cpuid_cache.has_fma;     }

__const
static inline int cpuid_has_pge(void)     { return cpuid_cache.has_pge;     }

__const
static inline int cpuid_has_pcid(void)    { return cpuid_cache.has_pcid;    }

__const
static inline int cpuid_has_invpcid(void) { return cpuid_cache.has_invpcid; }

__const
static inline int cpuid_has_sse4_1(void)  { return cpuid_cache.has_sse4_1;  }

__const
static inline int cpuid_has_sse4_2(void)  { return cpuid_cache.has_sse4_2;  }

__const
static inline int cpuid_has_x2apic(void)  { return cpuid_cache.has_x2apic;  }

__const
static inline int cpuid_has_aes(void)     { return cpuid_cache.has_aes;     }

__const
static inline int cpuid_has_xsave(void)   { return cpuid_cache.has_xsave;   }

__const
static inline int cpuid_has_avx(void)     { return cpuid_cache.has_avx;     }

__const
static inline int cpuid_has_rdrand(void)  { return cpuid_cache.has_rdrand;  }

__const
static inline int cpuid_has_smep(void)    { return cpuid_cache.has_smep;    }

__const
static inline int cpuid_has_de(void)      { return cpuid_cache.has_de;      }
