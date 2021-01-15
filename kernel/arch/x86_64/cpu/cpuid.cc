#define CPUID_CC
#include "./cpuid.h"
#include "string.h"
#include "likely.h"
#include "control_regs_constants.h"
#include "assert.h"

// max_leaf[0] holds max supported leaf
// max_leaf[1] holds max supported extended leaf
static uint32_t max_leaf[2];

cpuid_cache_t cpuid_cache;
int cpuid_nx_mask;

static char const sig_hv_tcg[12] = {
    'T', 'C', 'G', 'T', 'C', 'G', 'T', 'C', 'G', 'T', 'C', 'G'
};

static char const sig_hv_kvm[12] = {
    'K', 'V', 'M', 'K', 'V', 'M', 'K', 'V', 'M', 0, 0, 0
};

void cpuid_init()
{
    cpuid_t info;

    if (cpuid(&info, CPUID_HIGHESTFUNC, 0)) {
        if (!memcmp(&info.ebx, "AuthenticAMD", 12))
            cpuid_cache.is_amd = 1;
        else if (!memcmp(&info.ebx, "GenuineIntel", 12))
            cpuid_cache.is_intel = 1;
    }

    if (cpuid(&info, CPUID_INFO_FEATURES, 0)) {
        uint8_t family_id = ((info.eax >> 8) & 0xF);
        uint8_t ext_family_id = ((info.eax >> 20) & 0xFF);

        /// "The actual processor family is derived from the Family ID
        /// and Extended Family ID fields. If the Family ID field is
        /// equal to 15, the family is equal to the sum of the
        /// Extended Family ID and the Family ID fields.
        /// Otherwise, the family is equal to value of the
        /// Family ID field." - Wikipedia CPUID page
        if (family_id == 0xF) {
            cpuid_cache.family = family_id + ext_family_id;
        } else {
            cpuid_cache.family = family_id;
        }

//        cpuid_cache.family          = ((info.eax >> 8) & 0xF) +
//                                        ((info.eax >> 20) & 0xFF);

        /// "If the Family ID field is either 6 or 15, the model is
        /// equal to the sum of the Extended Model ID field
        /// shifted left by 4 bits and the Model field.
        /// Otherwise, the model is equal to the value of the
        /// Model field." - Wikipedia CPUID page
        uint8_t model_id = ((info.eax >> 4) & 0xF);
        uint8_t ext_model_id = (((info.eax >> 16) & 0xFF) << 4);

        if (model_id == 0x6 || model_id == 0xF)
            cpuid_cache.model = ext_model_id + model_id;
        else
            cpuid_cache.model = model_id;

        cpuid_cache.has_de          = info.edx & (1U << 2);
        cpuid_cache.has_pge         = info.edx & (1U << 13);
        cpuid_cache.has_sysenter    = info.edx & (1U << 11);
        cpuid_cache.has_pat         = info.edx & (1U << 16);
        cpuid_cache.has_self_snoop  = info.edx & (1U << 16);
        cpuid_cache.has_clflush     = info.edx & (1U << 19);

        cpuid_cache.has_sse3        = info.ecx & (1U << 0);
        cpuid_cache.has_mwait       = info.ecx & (1U << 3);
        cpuid_cache.has_ssse3       = info.ecx & (1U << 9);
        cpuid_cache.has_fma         = info.ecx & (1U << 12);
        cpuid_cache.has_pcid        = info.ecx & (1U << 17);
        cpuid_cache.has_sse4_1      = info.ecx & (1U << 19);
        cpuid_cache.has_sse4_2      = info.ecx & (1U << 20);
        cpuid_cache.has_x2apic      = info.ecx & (1U << 21);
        cpuid_cache.has_aes         = info.ecx & (1U << 25);
        cpuid_cache.has_xsave       = info.ecx & (1U << 26);
        cpuid_cache.has_avx         = info.ecx & (1U << 28);
        cpuid_cache.has_rdrand      = info.ecx & (1U << 30);
        cpuid_cache.is_hypervisor   = info.ecx & (1U << 31);
    }

    if (cpuid_cache.has_xsave && cpuid(&info, CPUID_INFO_XSAVE, 1)) {
        cpuid_cache.has_xsaves      = info.eax & (1U << 3);
        cpuid_cache.has_xsaveopt    = info.eax & (1U << 0);
        cpuid_cache.has_xsavec      = info.eax & (1U << 1);
    }

    if (cpuid(&info, CPUID_EXTINFO_FEATURES, 0)) {
        cpuid_cache.has_2mpage  = info.edx & (1U << 3);
        cpuid_cache.has_1gpage  = info.edx & (1U << 26);
        cpuid_cache.has_nx      = info.edx & (1U << 20);
        cpuid_cache.has_perfctr = info.ecx & (1U << 23);
    }

    if (cpuid(&info, CPUID_INFO_EXT_FEATURES, 0)) {
        cpuid_cache.has_umip     = info.ecx & (1U << 2);

        cpuid_cache.has_fsgsbase = info.ebx & (1U << 0);
        cpuid_cache.has_smep     = info.ebx & (1U << 7);
        cpuid_cache.has_erms     = info.ebx & (1U << 9);
        cpuid_cache.has_invpcid  = info.ebx & (1U << 10);
        cpuid_cache.has_avx512f  = info.ebx & (1U << 16);
        cpuid_cache.has_smap     = info.ebx & (1U << 20);

        cpuid_cache.has_md_clear = info.edx & (1U << 10);
        cpuid_cache.has_ibrs     = info.edx & (1U << 26);
        cpuid_cache.has_stibp    = info.edx & (1U << 27);
        cpuid_cache.has_ssbd     = info.edx & (1U << 31);
        cpuid_cache.has_l1df     = info.edx & (1U << 28);
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
        cpuid_cache.paddr_bits = info.eax & 0xFF;
        cpuid_cache.laddr_bits = (info.eax >> 8) & 0xFF;

        cpuid_cache.has_clzero = info.ebx & (1U << 0);
    } else {
        // Make reasonable guess
        cpuid_cache.paddr_bits = 52;
        cpuid_cache.laddr_bits = 48;
    }

    if (cpuid_cache.is_intel)
        cpuid_cache.bug_meltdown = true;

    if (cpuid_is_hypervisor() && cpuid(&info, CPUID_HYPERVISOR, 0)) {
        char str[12];
        memcpy(str + 0, &info.ebx, 4);
        memcpy(str + 4, &info.ecx, 4);
        memcpy(str + 8, &info.edx, 4);

        C_ASSERT(sizeof(sig_hv_kvm) == 12);
        C_ASSERT(sizeof(sig_hv_tcg) == 12);

        if (!memcmp(sig_hv_kvm, str, 12))
            cpuid_cache.hv_type = hv_type_t::KVM;
        else if (!memcmp(sig_hv_tcg, str, 12))
            cpuid_cache.hv_type = hv_type_t::TCG;
        else
            cpuid_cache.hv_type = hv_type_t::UNKNOWN;
    } else {
        cpuid_cache.hv_type = hv_type_t::NONE;
    }

    char *brand = cpuid_cache.brand;
    for (size_t i = CPUID_BRANDSTR1; i <= CPUID_BRANDSTR3; ++i) {
        if (likely(cpuid(&info, i, 0))) {
            memcpy(brand + sizeof(uint32_t) * 0, &info.eax, sizeof(info.eax));
            memcpy(brand + sizeof(uint32_t) * 1, &info.ebx, sizeof(info.ebx));
            memcpy(brand + sizeof(uint32_t) * 2, &info.ecx, sizeof(info.ecx));
            memcpy(brand + sizeof(uint32_t) * 3, &info.edx, sizeof(info.edx));
            brand += sizeof(uint32_t) * 4;
        }
    }

    if (unlikely(brand != cpuid_cache.brand + sizeof(cpuid_cache.brand)))
        memset(cpuid_cache.brand, 0, sizeof(cpuid_cache.brand));
}

int cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx)
{
    // Automatically check for support for the leaf
    // Don't cache CPUID_INFO_FEATURES because it has per-cpu value in ebx
    if ((eax & 0xF0000000) != 0x40000000 &&
            ((eax & 0x7FFFFFFF) != 0) &&
            (eax != CPUID_INFO_FEATURES)) {
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
