#include "cpuid.h"
#include "likely.h"

// Returns true if the CPU supports that leaf
// Hypervisor leaves are asinine and it always returns true for them
bool cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx)
{
    // 0x4000xxxx (hypervisor) leaves are utterly ridiculous
    if ((eax & 0xF0000000) != 0x40000000) {
        // Automatically check for support for the leaf
        if ((eax & 0x1FFFFFFF) != 0) {
            cpuid(output, eax & 0xE0000000U, 0);
            if (output->eax < eax)
                return false;
        }
    }

    output->eax = eax;
    output->ecx = ecx;
    __asm__ __volatile__ (
        "cpuid"
        : "+a" (output->eax), "+c" (output->ecx)
        , "=d" (output->edx), "=b" (output->ebx)
        :
        : "memory"
    );

    return true;
}

bool cpu_has_long_mode()
{
    static int has;

    cpuid_t cpuinfo;
    has = (cpuid(&cpuinfo, 0x80000001U, 0) &&
            (cpuinfo.edx & (1<<29)))
            ? 1 : -1;

    return has > 0;
}

bool cpu_has_no_execute()
{
    static int has;

    if (has)
        return has > 0;

    cpuid_t cpuinfo;
    has = (cpuid(&cpuinfo, 0x80000001U, 0) &&
            (cpuinfo.edx & (1<<20)))
            ? 1 : -1;

    return has > 0;
}

bool cpu_has_global_pages()
{
    static int has;

    if (likely(has))
        return has > 0;

    cpuid_t cpuinfo;
    has = (cpuid(&cpuinfo, 1U, 0) &&
            (cpuinfo.edx & (1<<13)))
            ? 1 : -1;

    return has > 0;
}

// Return true if the ABM, BMI1, BMI2
bool cpu_has_bmi()
{
    cpuid_t info;

    static int has;

    if (likely(has))
        return has > 0;

    bool result = true;

    // ABM=CPUID[0x80000001].ecx[5]
    // BMI1=CPUID[7].ebx[3]
    // BMI2=CPUID[7].ebx[8]
    if (unlikely(!cpuid(&info, 0x80000001, 0)))
        result = false;
    else if (unlikely(!(info.ecx & (1 << 5))))
        result = false;
    else if (unlikely(!cpuid(&info, 7, 0)))
        result = false;
    else if (unlikely(!(info.ebx & (1 << 3))))
        result = false;
    else if (unlikely(!(info.ebx & (1 << 8))))
        result = false;

    has = result ? 1 : -1;

    return has > 0;
}

//// Return true if the CPU supports:
////  sse, sse2, sse3, ssse3, sse4, sse4.1, sse4.2
//static bool cpu_has_upto_sse42()
//{
//    cpuid_t cpuinfo;

//    static int has;

//    if (has)
//        return has > 0;

//    if (!cpuid(&cpuinfo, 1U, 0)) {
//        has = -1;
//        return false;
//    }

//    // bit0=sse3, bit9=ssse3, bit19=sse4.1, bit20=sse4.2
//    has = ((cpuinfo.ecx & (1U<<0)) &&
//           (cpuinfo.ecx & (1U<<9)) &&
//           (cpuinfo.ecx & (1U<<19)) &&
//           (cpuinfo.ecx & (1U<<20)))
//            ? 1 : -1;

//    return has > 0;
//}

// Return true if the CPU supports avx, avx2
bool cpu_has_upto_avx2()
{
    cpuid_t cpuinfo;

    if (unlikely(!cpuid(&cpuinfo, 7U, 0)))
        return false;

    // bit5=avx2
    if (unlikely(!(cpuinfo.ebx & (1U<<5))))
        return false;

    if (unlikely(!cpuid(&cpuinfo, 1U, 0)))
        return false;

    if (unlikely(!(cpuinfo.ecx & (1U<<28))))
        return false;

    if (unlikely(!cpuid(&cpuinfo, 0xD, 0)))
        return false;

    return true;
}
