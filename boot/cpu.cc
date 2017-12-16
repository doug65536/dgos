#include "cpu.h"
#include "paging.h"
#include "bootsect.h"
#include "exception.h"
#include "farptr.h"
#include "string.h"
#include "gdt.h"
#include "bioscall.h"
#include "screen.h"

extern gdt_entry_t gdt[];

table_register_64_t idtr_64;
table_register_t idtr_16 __used = {
    0,
    4 * 256,
    0
};

idt_entry_64_t idt[32];

bool toggle_a20(uint8_t enable)
{
    enum struct a20_method {
        unknown,
        bios,
        port92,
        keybd,
        unspecified
    };

    static a20_method method = a20_method::unknown;

    bios_regs_t regs{};

    if (method == a20_method::unknown) {
        // Try to use the BIOS
        regs.eax = enable ? 0x2401 : 0x2400;
        bioscall(&regs, 0x15);
        if (!regs.flags_CF()) {
            method = a20_method::bios;
            return true;
        }

        // Ask the BIOS which method to use
        regs.eax = 0x2403;
        bioscall(&regs, 0x15);
        if (!regs.flags_CF() && (regs.ebx & 3)) {
            if (regs.ebx & 1) {
                method = a20_method::keybd;
            } else {
                method = a20_method::port92;
            }
        } else {
            print_line("BIOS doesn't support A20! Guessing port 0x92...");
            method = a20_method::port92;
        }
    }

    switch (method) {
    case a20_method::bios:
        regs.eax = enable ? 0x2401 : 0x2400;
        bioscall(&regs, 0x15);
        return !regs.flags_CF();

    case a20_method::port92:
        uint8_t value;
        enable = (enable != 0) << 1;
        __asm__ __volatile__ (
            "inb $0x92,%[value]\n\t"
            "andb $~2,%[value]\n\t"
            "orb %[enable],%[value]\n\t"
            "wbinvd\n\t"
            "outb %1,$0x92\n\t"
            : [enable] "+c" (enable)
            , [value] "=a" (value)
        );
        return true;

    case a20_method::keybd:
        // Command write
        while (inb(0x64) & 2);
        outb(0x64, 0xD1);

        // Write command
        while (inb(0x64) & 2);
        outb(0x60, enable ? 0xDF : 0xDD);

        // Wait for empty and cover signal propagation delay
        while (inb(0x64) & 2);

        return true;

    default:
        return false;

    }
}

// Returns true if the CPU supports that leaf
bool cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx)
{
    // Automatically check for support for the leaf
    if ((eax & 0x7FFFFFFF) != 0) {
        cpuid(output, eax & 0x80000000U, 0);
        if (output->eax < eax)
            return false;
    }

    __asm__ __volatile__ (
        "cpuid"
        : "=a" (output->eax), "=c" (output->ecx)
        , "=d" (output->edx), "=b" (output->ebx)
        : "a" (eax), "c" (ecx)
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

    if (has)
        return has > 0;

    cpuid_t cpuinfo;
    has = (cpuid(&cpuinfo, 1U, 0) &&
            (cpuinfo.edx & (1<<13)))
            ? 1 : -1;

    return has > 0;
}

// Return true if the CPU supports:
//  sse, sse2, sse3, ssse3, sse4, sse4.1, sse4.2
static bool cpu_has_upto_sse42()
{
    cpuid_t cpuinfo;

    static int has;

    if (has)
        return has > 0;

    if (!cpuid(&cpuinfo, 1U, 0)) {
        has = -1;
        return false;
    }

    // bit0=sse3, bit9=ssse3, bit19=sse4.1, bit20=sse4.2
    has = ((cpuinfo.ecx & (1U<<0)) &&
           (cpuinfo.ecx & (1U<<9)) &&
           (cpuinfo.ecx & (1U<<19)) &&
           (cpuinfo.ecx & (1U<<20)))
            ? 1 : -1;

    return has > 0;
}

static __always_inline uintptr_t cpu_cr4_change_bits(
        uintptr_t clear, uintptr_t set)
{
    uintptr_t rax;
    __asm__ __volatile__ (
        "mov %%cr4,%[result]\n\t"
        "and %[clear],%[result]\n\t"
        "or %[set],%[result]\n\t"
        "mov %[result],%%cr4\n\t"
        : [result] "=&r" (rax)
        : [clear] "ri" (~clear)
        , [set] "ri" (set)
        : "memory"
    );
    return rax;
}

static __always_inline uint64_t cpu_xcr_change_bits(
        uint32_t xcr, uint64_t clear, uint64_t set)
{
    clear = ~clear;
    uint64_t result;
    __asm__ __volatile__ (
        "xgetbv\n\t"
        "and (%[clear_mask]),%%eax\n\t"
        "and 4(%[clear_mask]),%%edx\n\t"
        "or (%[set]),%%eax\n\t"
        "or 4(%[set]),%%edx\n\t"
        "xsetbv\n\t"
        : "=&A" (result)
        : "c" (xcr)
        , [set] "r" (&set)
        , [clear_mask] "r" (&clear)
        : "memory"
    );
    return result;
}

#define XCR0_X87_BIT            0   // x87 FPU state
#define XCR0_SSE_BIT            1   // SSE state
#define XCR0_AVX_BIT            2   // AVX state
#define XCR0_MPX_BNDREG_BIT     3   // Memory Protection BNDREGS
#define XCR0_MPX_BNDCSR_BIT     4   // Memory Protection BNDCSR
#define XCR0_AVX512_OPMASK_BIT  5   // AVX-512 opmask registers k0-k7
#define XCR0_AVX512_UPPER_BIT   6   // AVX-512 upper 256 bits
#define XCR0_AVX512_XREGS_BIT   7   // AVX-512 extra 16 registers
#define XCR0_PT_BIT             8   // Processor Trace MSRs
#define XCR0_PKRU_BIT           9   // Protection Key

#define XCR0_X87                (1<<XCR0_X87_BIT)
#define XCR0_SSE                (1<<XCR0_SSE_BIT)
#define XCR0_AVX                (1<<XCR0_AVX_BIT)
#define XCR0_MPX_BNDREG         (1<<XCR0_MPX_BNDREG_BIT)
#define XCR0_MPX_BNDCSR         (1<<XCR0_MPX_BNDCSR_BIT)
#define XCR0_AVX512_OPMASK      (1<<XCR0_AVX512_OPMASK_BIT)
#define XCR0_AVX512_UPPER       (1<<XCR0_AVX512_UPPER_BIT)
#define XCR0_AVX512_XREGS       (1<<XCR0_AVX512_XREGS_BIT)
#define XCR0_PT                 (1<<XCR0_PT_BIT)
#define XCR0_PKRU               (1<<XCR0_PKRU_BIT)

// Return true if the CPU supports avx, avx2
bool cpu_has_upto_avx2()
{
    cpuid_t cpuinfo;

    if (!cpuid(&cpuinfo, 7U, 0))
        return false;

    // bit5=avx2
    if (!(cpuinfo.ebx & (1U<<5)))
        return false;

    if (!cpuid(&cpuinfo, 1U, 0))
        return false;

    if (!(cpuinfo.ecx & (1U<<28)))
        return false;

    if (!cpuid(&cpuinfo, 0xD, 0))
        return false;

    return true;
}

const char *cpu_choose_kernel()
{
    if (cpu_has_upto_sse42()) {
        if (cpu_has_upto_avx2())
            return "dgos-kernel-avx2";
        else
            return "dgos-kernel-sse4";
    } else
        return "dgos-kernel-generic";
}

bool need_a20_toggle;

bool nx_available;
uint32_t gp_available;

void cpu_init()
{
    nx_available = cpu_has_no_execute();
    gp_available = cpu_has_global_pages() ? (1 << 7) : 0;
}

void cpu_a20_enterpm()
{
    if (need_a20_toggle)
        toggle_a20(1);
}

void cpu_a20_exitpm()
{
    if (need_a20_toggle)
        toggle_a20(0);
}

// If src is > 0, copy size bytes from src to address
// If src is == 0, jumps to entry point in address, passing size as an argument
void copy_or_enter(uint64_t address, uint32_t src, uint32_t size)
{
    uint32_t pdbr = paging_root_addr();

    struct params_t {
        uint64_t addr;
        uint32_t src;
        uint32_t size;
        uint32_t pdbr;
    } params = {
        address,
        src,
        size,
        pdbr
    };

    __asm__ __volatile__ (
        // Enable CR4.PAE (bit 5)
        "movl %%cr4,%%eax\n\t"
        "btsl $%c[cr4_pae_bit],%%eax\n\t"
        "orl %[gp_available],%%eax\n\t"
        "movl %%eax,%%cr4\n\t"

        // Load PDBR
        "movl %c[pdbr_ofs](%[params]),%%eax\n\t"
        "movl %%eax,%%cr3\n\t"

        // Enable long mode IA32_EFER.LME (bit 8) MSR 0xC0000080
        // and if available, enable no-execute bit in paging
        "movl $%c[msr_efer],%%ecx\n"
        "rdmsr\n\t"
        "btsl $%c[msr_efer_lme_bit],%%eax\n\t"
        "cmpw $0,%[nx_available]\n\t"
        "jz 0f\n\t"
        "btsl $%c[msr_efer_nx_bit],%%eax\n\t"
        "0:"
        "wrmsr\n\t"

        // Enable paging (CR0.PG (bit 31)
        "movl %%cr0,%%eax\n\t"
        "btsl $%c[cr0_pg_bit],%%eax\n\t"
        "movl %%eax,%%cr0\n\t"

        "jmp 0f\n\t"
        "0:"

        // Now in 64 bit compatibility mode (still really 32 bit)

        // Far jump to selector that has L bit set (64 bit)
        "lea 6+idtr_64,%%eax\n\t"
        "lcall %[gdt_code64],$0f\n\t"

        // Returned from long mode, back to compatibility mode

        // Load 32 bit data segments
        "movl %[gdt_data32],%%eax\n\t"
        "movw %%ax,%%ds\n\t"
        "movw %%ax,%%es\n\t"
        "movw %%ax,%%fs\n\t"
        "movw %%ax,%%ss\n\t"

        // Jump over 64 bit code
        "jmp 1f\n\t"

        // Long mode
        ".code64\n\t"
        "0:\n\t"

        "lidtq (%%eax)\n\t"

        // Load 64 bit data segments
        "movl %[gdt_data64],%%eax\n\t"
        "movl %%eax,%%ds\n\t"
        "movl %%eax,%%es\n\t"
        "movl %%eax,%%fs\n\t"
        "movl %%eax,%%ss\n\t"

        // Load copy/entry parameters
        "mov %c[addr_ofs](%[params]),%%rdi\n\t"
        "movl %c[src_ofs](%[params]),%%esi\n\t"
        "movl %c[size_ofs](%[params]),%%ecx\n\t"

        // Check whether it is copy or entry
        "testl %%esi,%%esi\n\t"
        "jz 2f\n\t"

        // Copy memory
        "cld\n\t"
        "rep movsb\n\t"
        "jmp 3f\n\t"

        //
        // Enter kernel
        "2:\n\t"
        "movb $'Y',0xb8000\n\t"  // <-- debug hack

        // Enable CR0.WP write protection
        "mov %%cr0,%%rax\n\t"
        "bts $%c[cr0_wp_bit],%%rax\n\t"
        "mov %%rax,%%cr0\n\t"

        "mov %%rsp,%%r15\n\t"
        "andq $-16,%%rsp\n\t"
        "call *%%rdi\n\t"
        // Should not be possible to reach here
        "mov %%r15,%%rsp\n\t"

        "3:\n\t"

        // Far return to 32 bit compatibility mode code segment
        "lret\n\t"

        "1:\n\t"
        ".code32\n\t"

        // Disable paging
        "mov %%cr0,%%eax\n\t"
        "btr $%c[cr0_pg_bit],%%eax\n\t"
        "mov %%eax,%%cr0\n\t"

        // Disable long mode
        "movl $%c[msr_efer],%%ecx\n"
        "rdmsr\n\t"
        "btr $%c[msr_efer_lme_bit],%%eax\n\t"
        "wrmsr\n\t"

        // Load 32 bit selectors
        "movl %[gdt_data32],%%eax\n\t"
        "movw %%ax,%%ds\n\t"
        "movw %%ax,%%es\n\t"
        "movw %%ax,%%fs\n\t"
        "movw %%ax,%%ss\n\t"

        // Load 32-bit code segment
        "ljmp %[gdt_code32],$0f\n\t"
        "nop\n\t"

        // 32-bit addressing mode reenabled
        ".code32\n\t"
        "0:\n\t"

        "lea 2+idtr_16,%%esi\n\t"
        "lidt (%%esi)\n\t"
        :
        : [params] "b" (&params)
        , [nx_available] "m" (nx_available)
        , [gp_available] "m" (gp_available)
        , [addr_ofs] "e" (offsetof(params_t, addr))
        , [src_ofs] "e" (offsetof(params_t, src))
        , [size_ofs] "e" (offsetof(params_t, size))
        , [pdbr_ofs] "e" (offsetof(params_t, pdbr))
        , [gdt_code64] "n" (GDT_SEL_KERNEL_CODE64)
        , [gdt_data64] "n" (GDT_SEL_KERNEL_DATA)
        , [gdt_code32] "n" (GDT_SEL_KERNEL_CODE32)
        , [gdt_data32] "n" (GDT_SEL_KERNEL_DATA32)
        , [gdt_code16] "n" (GDT_SEL_KERNEL_CODE16)
        , [gdt_data16] "n" (GDT_SEL_KERNEL_DATA16)
        , [msr_efer] "n" (CPU_MSR_EFER)
        , [cr0_pg_bit] "n" (CPU_CR0_PG_BIT)
        , [cr0_wp_bit] "n" (CPU_CR0_WP_BIT)
        , [cr4_pae_bit] "n" (CPU_CR4_PAE_BIT)
        , [msr_efer_lme_bit] "n" (CPU_MSR_EFER_LME_BIT)
        , [msr_efer_nx_bit] "n" (CPU_MSR_EFER_NX_BIT)
        : "eax", "ecx", "edx", "esi", "edi", "memory"
    );
}
