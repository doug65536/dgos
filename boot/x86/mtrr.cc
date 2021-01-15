#include "mtrr.h"
#include "likely.h"
#include "cpuid.h"
#include "string.h"
#include "assert.h"

static _always_inline uint64_t cpu_msr_get(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ __volatile__ (
        "rdmsr\n\t"
        : "=a" (lo)
        , "=d" (hi)
        : "c" (msr)
    );
    return ((uint64_t)hi << 32) | lo;
}

#define IA32_MTRRCAP    0xFE

#define IA32_MTRR_PHYSBASE_n(n) (0x200 + ((n)*2))
#define IA32_MTRR_PHYSMASK_n(n) (0x201 + ((n)*2))
#define IA32_MTRR_FIX64K_00000  0x250
#define IA32_MTRR_FIX16K_80000  0x258
#define IA32_MTRR_FIX16K_A0000  0x259
#define IA32_MTRR_FIX4K_C0000   0x268
#define IA32_MTRR_FIX4K_C8000   0x269
#define IA32_MTRR_FIX4K_D0000   0x26A
#define IA32_MTRR_FIX4K_D8000   0x26B
#define IA32_MTRR_FIX4K_E0000   0x26C
#define IA32_MTRR_FIX4K_E8000   0x26D
#define IA32_MTRR_FIX4K_F0000   0x26E
#define IA32_MTRR_FIX4K_F8000   0x26F
#define IA32_MTRR_DEF_TYPE      0x2FF
#define IA32_MTRR_DEF_TYPE_MASK     0x3
#define IA32_MTRR_DEF_TYPE_FIXED_EN (1<<10)
#define IA32_MTRR_DEF_TYPE_MTRR_EN  (1<<11)

__BEGIN_ANONYMOUS

#define IA32_MTRR_MEMTYPE_UC 0
#define IA32_MTRR_MEMTYPE_WC 1
#define IA32_MTRR_MEMTYPE_WT 4
#define IA32_MTRR_MEMTYPE_WP 5
#define IA32_MTRR_MEMTYPE_WB 6

struct mtrr_info_t {
    // Fixed MTRRs are packed, 8 memory type (bytes) per MSR
    // 8 64KB regions, 512KB, 1 MSR
    // 16 16KB regions, 256KB, 2 MSRs
    // 64 4KB regions, 256KB, 8 MSRs
    // Variable MTRRs are two MSRs each, base_en and mask
    // 8 variable ranges, 16 MSRs

    // 8 64KB, 16 16KB, 64 4KB = 88 memory types in 11 MSRs
    uint8_t fixed_types[8+16+64] = {};

    uint64_t fixed_bases[16] = {};
    uint64_t fixed_masks[16] = {};

    uint8_t vcnt = 0;
    uint8_t def_type = 0;
    bool enabled_fixed = false;
    bool enabled_mtrr = false;
    bool support_wc = false;
    bool support_smrr = false;

    mtrr_info_t();
    mtrr_info_t(mtrr_info_t const&) = default;
    mtrr_info_t &operator=(mtrr_info_t const&) = default;
    ~mtrr_info_t() = default;

    uint8_t memtype_of(uint64_t addr)
    {
        uint64_t byte_index;

        if (addr < 0x100000) {
            if (addr < 0x80000) {
                byte_index = addr - 0x80000;
                byte_index >>= 16;
            } else if (addr < 0xC0000) {
                byte_index = addr - 0xA0000;
                byte_index >>= 14;
                byte_index += 8;
            } else {
                byte_index = addr - 0xC0000;
                byte_index >>= 12;
                byte_index += 8 + 16;
            }

            return fixed_types[byte_index];
        }

        for (size_t i = 0; i < vcnt; ++i) {
            bool valid = fixed_masks[i] & (1<<11);

            if (unlikely(!valid))
                continue;

            uint64_t mask = fixed_masks[i] & -(1 << 12);

            uint8_t type = fixed_bases[i] & 0xFF;
            uint64_t base = fixed_bases[i] & -(1 << 12);

            uint64_t mask_base = mask & base;
            uint64_t mask_target = mask & (addr & -(1 << 12));

            if (mask_base == mask_target)
                return type;
        }

        return def_type;
    }
};

mtrr_info_t::mtrr_info_t()
{
    //
    // See if MTRR supported at all

    cpuid_t info;

    if (unlikely(!cpuid(&info, 1, 0)))
        return;

    if (unlikely(!(info.edx & (1 << 12))))
        return;

    uint64_t reg;

    //
    // Read MTRR capabilities

    reg = cpu_msr_get(IA32_MTRRCAP);

    vcnt = reg & 0xFF;
    bool support_fixed = reg & (1 << 8);
    support_wc = reg & (1 << 10);
    support_smrr = reg & (1 << 11);

    reg = cpu_msr_get(IA32_MTRR_DEF_TYPE);

    def_type = reg & IA32_MTRR_DEF_TYPE_MASK;
    enabled_fixed = support_fixed && (reg & IA32_MTRR_DEF_TYPE_FIXED_EN);
    enabled_mtrr = reg & IA32_MTRR_DEF_TYPE_MTRR_EN;

    if (enabled_fixed) {
        reg = cpu_msr_get(IA32_MTRR_FIX64K_00000);
        memcpy(fixed_types + 0 * sizeof(reg), &reg, sizeof(reg));

        reg = cpu_msr_get(IA32_MTRR_FIX16K_80000);
        memcpy(fixed_types + 1 * sizeof(reg), &reg, sizeof(reg));

        reg = cpu_msr_get(IA32_MTRR_FIX16K_A0000);
        memcpy(fixed_types + 2 * sizeof(reg), &reg, sizeof(reg));

        reg = cpu_msr_get(IA32_MTRR_FIX4K_C0000);
        memcpy(fixed_types + 3 * sizeof(reg), &reg, sizeof(reg));

        reg = cpu_msr_get(IA32_MTRR_FIX4K_C8000);
        memcpy(fixed_types + 4 * sizeof(reg), &reg, sizeof(reg));

        reg = cpu_msr_get(IA32_MTRR_FIX4K_D0000);
        memcpy(fixed_types + 5 * sizeof(reg), &reg, sizeof(reg));

        reg = cpu_msr_get(IA32_MTRR_FIX4K_D8000);
        memcpy(fixed_types + 6 * sizeof(reg), &reg, sizeof(reg));

        reg = cpu_msr_get(IA32_MTRR_FIX4K_E0000);
        memcpy(fixed_types + 7 * sizeof(reg), &reg, sizeof(reg));

        reg = cpu_msr_get(IA32_MTRR_FIX4K_E8000);
        memcpy(fixed_types + 8 * sizeof(reg), &reg, sizeof(reg));

        reg = cpu_msr_get(IA32_MTRR_FIX4K_F0000);
        memcpy(fixed_types + 9 * sizeof(reg), &reg, sizeof(reg));

        reg = cpu_msr_get(IA32_MTRR_FIX4K_F8000);
        memcpy(fixed_types + 10 * sizeof(reg), &reg, sizeof(reg));
    }

    if (vcnt) {
        assert(vcnt < 16);

        for (size_t i = 0; i < vcnt; ++i) {
            fixed_bases[i] = cpu_msr_get(IA32_MTRR_PHYSBASE_n(i));
            fixed_masks[i] = cpu_msr_get(IA32_MTRR_PHYSMASK_n(i));
        }
    }
}

__END_ANONYMOUS
