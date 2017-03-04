#pragma once

#include "types.h"

#define MSR_FSBASE      0xC0000100
#define MSR_GSBASE      0xC0000101
#define MSR_KGSBASE     0xC0000102
#define MSR_EFER        0xC0000080

#define MSR_IA32_MISC_ENABLES   0x1A0

// PAT MSR

#define MSR_IA32_PAT    0x277

// Uncacheable
#define MSR_IA32_PAT_UC     0

// Write Combining
#define MSR_IA32_PAT_WC     1

// Write Through
#define MSR_IA32_PAT_WT     4

// Write Protected
#define MSR_IA32_PAT_WP     5

// Writeback
#define MSR_IA32_PAT_WB     6

// Uncacheable and allow MTRR override
#define MSR_IA32_PAT_UCW    7

#define MSR_IA32_PAT_n(n,v) ((uint64_t)(v) << ((n) << 3))

#define CR0_PE_BIT 0	// Protected Mode
#define CR0_MP_BIT 1	// Monitor co-processor
#define CR0_EM_BIT 2	// Emulation
#define CR0_TS_BIT 3	// Task switched
#define CR0_ET_BIT 4	// Extension type
#define CR0_NE_BIT 5	// Numeric error
#define CR0_WP_BIT 16	// Write protect
#define CR0_AM_BIT 18	// Alignment mask
#define CR0_NW_BIT 29	// Not-write through
#define CR0_CD_BIT 30	// Cache disable
#define CR0_PG_BIT 31	// Paging

#define CR0_PE  (1 << CR0_PE_BIT)
#define CR0_MP  (1 << CR0_MP_BIT)
#define CR0_EM  (1 << CR0_EM_BIT)
#define CR0_TS  (1 << CR0_TS_BIT)
#define CR0_ET  (1 << CR0_ET_BIT)
#define CR0_NE  (1 << CR0_NE_BIT)
#define CR0_WP  (1 << CR0_WP_BIT)
#define CR0_AM  (1 << CR0_AM_BIT)
#define CR0_NW  (1 << CR0_NW_BIT)
#define CR0_CD  (1 << CR0_CD_BIT)
#define CR0_PG  (1 << CR0_PG_BIT)

#define CR4_VME_BIT         0	// Virtual 8086 Mode Extensions
#define CR4_PVI_BIT         1	// Protected-mode Virtual Interrupts
#define CR4_TSD_BIT         2	// Time Stamp Disable
#define CR4_DE_BIT          3	// Debugging Extensions
#define CR4_PSE_BIT         4	// Page Size Extension
#define CR4_PAE_BIT         5	// Physical Address Extension
#define CR4_MCE_BIT         6	// Machine Check Exception
#define CR4_PGE_BIT         7	// Page Global Enabled
#define CR4_PCE_BIT         8	// Performance-Monitoring Counter
#define CR4_OFXSR_BIT       9	// OS support for FXSAVE and FXRSTOR
#define CR4_OSXMMEX_BIT     10	// OS Support for Unmasked SIMD Floating-Point Exceptions
#define CR4_VMXE_BIT        13	// Virtual Machine Extensions Enable
#define CR4_SMXE_BIT        14	// Safer Mode Extensions Enable
#define CR4_FSGSBASE_BIT    16	// Enables instructions RDFSBASE, RDGSBASE, WRFSBASE, and WRGSBASE
#define CR4_PCIDE_BIT       17	// PCID Enable
#define CR4_OSXSAVE_BIT     18	// XSAVE
#define CR4_SMEP_BIT        20	// Supervisor Mode Execution Protection Enable
#define CR4_SMAP_BIT        21	// Supervisor Mode Access Protection Enable
#define CR4_PKE_BIT         22	// Protection Key Enable

#define CR4_VME             (1 << CR4_VME_BIT     )
#define CR4_PVI             (1 << CR4_PVI_BIT     )
#define CR4_TSD             (1 << CR4_TSD_BIT     )
#define CR4_DE              (1 << CR4_DE_BIT      )
#define CR4_PSE             (1 << CR4_PSE_BIT     )
#define CR4_PAE             (1 << CR4_PAE_BIT     )
#define CR4_MCE             (1 << CR4_MCE_BIT     )
#define CR4_PGE             (1 << CR4_PGE_BIT     )
#define CR4_PCE             (1 << CR4_PCE_BIT     )
#define CR4_OFXSR           (1 << CR4_OFXSR_BIT   )
#define CR4_OSXMMEX         (1 << CR4_OSXMMEX_BIT )
#define CR4_VMXE            (1 << CR4_VMXE_BIT    )
#define CR4_SMXE            (1 << CR4_SMXE_BIT    )
#define CR4_FSGSBASE        (1 << CR4_FSGSBASE_BIT)
#define CR4_PCIDE           (1 << CR4_PCIDE_BIT   )
#define CR4_OSXSAVE         (1 << CR4_OSXSAVE_BIT )
#define CR4_SMEP            (1 << CR4_SMEP_BIT    )
#define CR4_SMAP            (1 << CR4_SMAP_BIT    )
#define CR4_PKE             (1 << CR4_PKE_BIT     )

typedef struct table_register_t {
    uint16_t align;
    uint16_t limit;
    uint32_t base;
} table_register_t;

typedef struct table_register_64_t {
    // Dummy 16-bit field for alignment
    uint8_t align[sizeof(uintptr_t)-sizeof(uint16_t)];

    // Actual beginning of register value
    uint16_t limit;
    uintptr_t base;
} table_register_64_t;

// Get whole MSR register as a 64-bit value
uint64_t msr_get(uint32_t msr);

// Get low 32 bits pf MSR register
uint32_t msr_get_lo(uint32_t msr);

// High high 32 bits of MSR register
uint32_t msr_get_hi(uint32_t msr);

// Set whole MSR as a 64 bit value
void msr_set(uint32_t msr, uint64_t value);

// Update the low 32 bits of MSR, preserving the high 32 bits
void msr_set_lo(uint32_t msr, uint32_t value);

// Update the low 32 bits of MSR, preserving the high 32 bits
void msr_set_hi(uint32_t msr, uint32_t value);

// Set the specified bit of the specified MSR
uint64_t msr_adj_bit(uint32_t msr, int bit, int set);

// Returns new value of cr0
uintptr_t cpu_cr0_change_bits(uintptr_t clear, uintptr_t set);

// Returns new value of cr0
uintptr_t cpu_cr4_change_bits(uintptr_t clear, uintptr_t set);

static inline uintptr_t cpu_get_fault_address(void)
{
    uintptr_t addr;
    __asm__ __volatile__ (
        "mov %%cr2,%[addr]\n\t"
        : [addr] "=r" (addr)
    );
    return addr;
}

uintptr_t cpu_get_page_directory(void);
void cpu_set_page_directory(uintptr_t addr);
void cpu_flush_tlb(void);

void cpu_set_fsbase(void *fs_base);
void cpu_set_gsbase(void *gs_base);

static inline void cpu_invalidate_page(uintptr_t addr)
{
    __asm__ __volatile__ (
        "invlpg (%[addr])\n\t"
        :
        : [addr] "r" (addr)
        : "memory"
    );
}

table_register_64_t cpu_get_gdtr(void);
void cpu_set_gdtr(table_register_64_t gdtr);

uint16_t cpu_get_tr(void);
void cpu_set_tr(uint16_t tr);

static inline void *cpu_gs_read_ptr(void)
{
    void *ptr;
    __asm__ __volatile__ (
        "mov %%gs:0,%[ptr]\n\t"
        : [ptr] "=r" (ptr)
    );
    return ptr;
}

static inline int cpu_irq_disable(void)
{
    uintptr_t rflags;
    __asm__ __volatile__ (
        "pushf\n\t"
        "pop %[rflags]\n\t"
        "cli\n\t"
        : [rflags] "=r" (rflags)
        :
        : "memory"
    );
    return ((rflags >> 9) & 1);
}

static inline void cpu_irq_enable(void)
{
    __asm__ __volatile__ ( "sti" : : : "memory" );
}

static inline void cpu_irq_toggle(int enable)
{
    uintptr_t temp;
    __asm__ __volatile__ (
        "pushfq\n\t"
        "pop %q[temp]\n\t"
        "and $~(1<<9),%k[temp]\n\t"
        "or %k[enable],%k[temp]\n\t"
        "push %q[temp]\n\t"
        "popfq\n\t"
        : [temp] "=&r" (temp)
        : [enable] "r" ((!!enable) << 9)
        : "memory"
    );
}

void *cpu_get_stack_ptr(void);
void cpu_crash(void);

static inline uint64_t cpu_rdtsc(void)
{
    uint32_t tsc_lo;
    uint32_t tsc_hi;
    __asm__ __volatile__ (
        "rdtsc\n\t"
        : "=a" (tsc_lo), "=d" (tsc_hi)
    );
    return tsc_lo | ((uint64_t)tsc_hi << 32);
}

uint32_t cpu_get_default_mxcsr_mask(void);
