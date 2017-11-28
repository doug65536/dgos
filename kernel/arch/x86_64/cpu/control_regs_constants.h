#pragma once

// This is included in assembly

#define MSR_FSBASE          0xC0000100
#define MSR_GSBASE          0xC0000101
#define MSR_KGSBASE         0xC0000102
#define MSR_EFER            0xC0000080

// RFLAGS mask on syscall entry
#define MSR_FMASK           0xC0000084

// Long mode syscall entry
#define MSR_LSTAR           0xC0000082

// CS/SS values for syscall entry
#define MSR_STAR            0xC0000081

// IA32_MISC_ENABLES
#define MSR_MISC_ENABLES    0x1A0

// PAT MSR
#define MSR_IA32_PAT        0x277

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

//
// XSAVE/XRSTOR

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


//
// CPU context

#define EFLAGS_CF_BIT   0
#define EFLAGS_PF_BIT   2
#define EFLAGS_AF_BIT   4
#define EFLAGS_ZF_BIT   6
#define EFLAGS_SF_BIT   7
#define EFLAGS_TF_BIT   8
#define EFLAGS_IF_BIT   9
#define EFLAGS_DF_BIT   10
#define EFLAGS_OF_BIT   11
#define EFLAGS_IOPL_BIT 12
#define EFLAGS_NT_BIT   14
#define EFLAGS_RF_BIT   16
#define EFLAGS_VM_BIT   17
#define EFLAGS_AC_BIT   18
#define EFLAGS_VIF_BIT  19
#define EFLAGS_VIP_BIT  20
#define EFLAGS_ID_BIT   21

#define EFLAGS_CF       (1 << EFLAGS_CF_BIT)
#define EFLAGS_PF       (1 << EFLAGS_PF_BIT)
#define EFLAGS_AF       (1 << EFLAGS_AF_BIT)
#define EFLAGS_ZF       (1 << EFLAGS_ZF_BIT)
#define EFLAGS_SF       (1 << EFLAGS_SF_BIT)
#define EFLAGS_TF       (1 << EFLAGS_TF_BIT)
#define EFLAGS_IF       (1 << EFLAGS_IF_BIT)
#define EFLAGS_DF       (1 << EFLAGS_DF_BIT)
#define EFLAGS_OF       (1 << EFLAGS_OF_BIT)
#define EFLAGS_NT       (1 << EFLAGS_NT_BIT)
#define EFLAGS_RF       (1 << EFLAGS_RF_BIT)
#define EFLAGS_VM       (1 << EFLAGS_VM_BIT)
#define EFLAGS_AC       (1 << EFLAGS_AC_BIT)
#define EFLAGS_VIF      (1 << EFLAGS_VIF_BIT)
#define EFLAGS_VIP      (1 << EFLAGS_VIP_BIT)
#define EFLAGS_ID       (1 << EFLAGS_ID_BIT)

#define EFLAGS_IOPL_BITS    2
#define EFLAGS_IOPL_MASK    ((1 << EFLAGS_IOPL_BITS)-1)
#define EFLAGS_IOPL         (EFLAGS_IOPL_MASK << EFLAGS_IOPL_BIT)

#define MXCSR_IE_BIT        0
#define MXCSR_DE_BIT        1
#define MXCSR_ZE_BIT        2
#define MXCSR_OE_BIT        3
#define MXCSR_UE_BIT        4
#define MXCSR_PE_BIT        5
#define MXCSR_DAZ_BIT       6
#define MXCSR_IM_BIT        7
#define MXCSR_DM_BIT        8
#define MXCSR_ZM_BIT        9
#define MXCSR_OM_BIT        10
#define MXCSR_UM_BIT        11
#define MXCSR_PM_BIT        12
#define MXCSR_RC_BIT        13
#define MXCSR_FZ_BIT        15

#define MXCSR_IE            (1 << MXCSR_IE_BIT)
#define MXCSR_DE            (1 << MXCSR_DE_BIT)
#define MXCSR_ZE            (1 << MXCSR_ZE_BIT)
#define MXCSR_OE            (1 << MXCSR_OE_BIT)
#define MXCSR_UE            (1 << MXCSR_UE_BIT)
#define MXCSR_PE            (1 << MXCSR_PE_BIT)
#define MXCSR_DAZ           (1 << MXCSR_DAZ_BIT)
#define MXCSR_IM            (1 << MXCSR_IM_BIT)
#define MXCSR_DM            (1 << MXCSR_DM_BIT)
#define MXCSR_ZM            (1 << MXCSR_ZM_BIT)
#define MXCSR_OM            (1 << MXCSR_OM_BIT)
#define MXCSR_UM            (1 << MXCSR_UM_BIT)
#define MXCSR_PM            (1 << MXCSR_PM_BIT)
#define MXCSR_FZ            (1 << MXCSR_FZ_BIT)

#define MXCSR_MASK_ALL      (MXCSR_IM | MXCSR_DM | MXCSR_ZM | \
                                MXCSR_OM | MXCSR_UM | MXCSR_PM)

#define MXCSR_RC_NEAREST    0
#define MXCSR_RC_DOWN       1
#define MXCSR_RC_UP         2
#define MXCSR_RC_TRUNCATE   3

#define MXCSR_RC_BITS       2
#define MXCSR_RC_MASK       ((1 << MXCSR_RC_BITS)-1)
#define MXCSR_RC            (MXCSR_RC_MASK << MXCSR_RC_BIT)
#define MXCSR_RC_n(rc)      (((rc) & MXCSR_RC_MASK) << MXCSR_RC_BIT)

#define MXCSR_ELF_INIT \
    MXCSR_RC_n(MXCSR_RC_NEAREST) | \
    MXCSR_MASK_ALL

//
// Floating point control word

#define FPUCW_IM_BIT        0
#define FPUCW_DM_BIT        1
#define FPUCW_ZM_BIT        2
#define FPUCW_OM_BIT        3
#define FPUCW_UM_BIT        4
#define FPUCW_PM_BIT        5
#define FPUCW_PC_BIT        8
#define FPUCW_RC_BIT        10

#define FPUCW_PC_BITS       2
#define FPUCW_RC_BITS       2

#define FPUCW_PC_MASK       ((1U<<FPUCW_PC_BITS)-1)
#define FPUCW_RC_MASK       ((1U<<FPUCW_RC_BITS)-1)

#define FPUCW_IM            (1U<<FPUCW_IM_BIT)
#define FPUCW_DM            (1U<<FPUCW_DM_BIT)
#define FPUCW_ZM            (1U<<FPUCW_ZM_BIT)
#define FPUCW_OM            (1U<<FPUCW_OM_BIT)
#define FPUCW_UM            (1U<<FPUCW_UM_BIT)
#define FPUCW_PM            (1U<<FPUCW_PM_BIT)
#define FPUCW_PC_n(n)       ((n)<<FPUCW_PC_BIT)
#define FPUCW_RC_n(n)       ((n)<<FPUCW_RC_BIT)

#define FPUCW_PC_24         0
#define FPUCW_PC_53         2
#define FPUCW_PC_64         3

#define FPUCW_RC_NEAREST    MXCSR_RC_NEAREST
#define FPUCW_RC_DOWN       MXCSR_RC_DOWN
#define FPUCW_RC_UP         MXCSR_RC_UP
#define FPUCW_RC_TRUNCATE   MXCSR_RC_TRUNCATE

#define FPUCW_ELF_INIT \
    (FPUCW_RC_n(FPUCW_RC_NEAREST) | \
    FPUCW_PC_n(FPUCW_PC_53) | \
    FPUCW_IM | FPUCW_DM | FPUCW_ZM | \
    FPUCW_OM | FPUCW_UM | FPUCW_PM)

//
// FPU Status Word

#define FPUSW_IE_BIT        0
#define FPUSW_DE_BIT        1
#define FPUSW_ZE_BIT        2
#define FPUSW_OE_BIT        3
#define FPUSW_UE_BIT        4
#define FPUSW_PE_BIT        5
#define FPUSW_SF_BIT        6
#define FPUSW_ES_BIT        7
#define FPUSW_C0_BIT        8
#define FPUSW_C1_BIT        9
#define FPUSW_C2_BIT        10
#define FPUSW_TOP_BIT       11
#define FPUSW_C3_BIT        14
#define FPUSW_B_BIT         15

#define FPUSW_TOP_BITS      2
#define FPUSW_TOP_MASK      ((1U<<FPUSW_TOP_BITS)-1)
#define FPUSW_TOP_n(n)      ((n)<<FPUSW_TOP_BIT)


#define CPUID_HIGHESTFUNC       0x0
#define CPUID_INFO_FEATURES     0x1
#define CPUID_CACHE_TLB         0x2
#define CPUID_SERIALNUM         0x3
#define CPUID_TOPOLOGY1         0x4
#define CPUID_TOPOLOGY2         0xB
#define CPUID_INFO_XSAVE        0xD
#define CPUID_EXTHIGHESTFUNC    0x80000000
#define CPUID_EXTINFO_FEATURES  0x80000001
#define CPUID_BRANDSTR1         0x80000002
#define CPUID_BRANDSTR2         0x80000003
#define CPUID_BRANDSTR3         0x80000004
#define CPUID_L1TLBIDENT        0x80000005
#define CPUID_EXTL2CACHE        0x80000006
#define CPUID_APM               0x80000007
#define CPUID_ADDRSIZES         0x80000008
