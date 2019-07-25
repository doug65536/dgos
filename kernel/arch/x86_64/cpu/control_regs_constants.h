#pragma once

// This is included in assembly

#define MAX_CPUS    64

#define CPU_MSR_FSBASE          0xC0000100U
#define CPU_MSR_GSBASE          0xC0000101U
#define CPU_MSR_KGSBASE         0xC0000102U
#define CPU_MSR_EFER            0xC0000080U
#define CPU_MSR_TSC_AUX         0xC0000103U

#define CPU_MSR_EFER_SCE_BIT    0
#define CPU_MSR_EFER_LME_BIT    8
#define CPU_MSR_EFER_LMA_BIT    10
#define CPU_MSR_EFER_NX_BIT     11
#define CPU_MSR_EFER_SVME_BIT   12
#define CPU_MSR_EFER_LMSLE_BIT  13
#define CPU_MSR_EFER_FFXSR_BIT  14
#define CPU_MSR_EFER_TCE_BIT    15

#define CPU_MSR_EFER_SCE        (1U << CPU_MSR_EFER_SCE_BIT)
#define CPU_MSR_EFER_LME        (1U << CPU_MSR_EFER_LME_BIT)
#define CPU_MSR_EFER_LMA        (1U << CPU_MSR_EFER_LMA_BIT)
#define CPU_MSR_EFER_NX         (1U << CPU_MSR_EFER_NX_BIT)
#define CPU_MSR_EFER_SVME       (1U << CPU_MSR_EFER_SVME_BIT)
#define CPU_MSR_EFER_LMSLE      (1U << CPU_MSR_EFER_LMSLE_BIT)
#define CPU_MSR_EFER_FFXSR      (1U << CPU_MSR_EFER_FFXSR_BIT)
#define CPU_MSR_EFER_TCE        (1U << CPU_MSR_EFER_TCE_BIT)

// RFLAGS mask on syscall entry
#define CPU_MSR_FMASK           0xC0000084U

// Compatibility mode syscall entry
#define CPU_MSR_CSTAR           0xC0000083U

// Long mode syscall entry
#define CPU_MSR_LSTAR           0xC0000082U

// CS/SS values for syscall entry
//             +-----------+--------------+--------------+
//             | 63      48 | 47       32 | 31         0 |
//             +------------+-------------+--------------+
//             | SYSRET_SEG | SYSCALL_SEG | 32 bit entry |
//             +------------+-------------+--------------+
//             |    User    |    Kernel   |
//   32 bit CS |     +0     |     +0      |
//   64 bit CS |    +16     |     +0      |
//   32 bit SS |     +8     |     +8      |
//   64 bit SS |     +8     |     +8      |
//             +------------+-------------+
//
// Required GDT layout:
//               usercode32
//                userdata
//               usercode64
//
//                           kernelcode64
//                            kerneldata
#define CPU_MSR_STAR            0xC0000081U

#define CPU_MSR_SPEC_CTRL       0x48

// IBRS if CPUID.(EAX=07H,ECX=0):EDX[26]=1
#define CPU_MSR_SPEC_CTRL_IBRS_BIT      0

// STIBP If CPUID.(EAX=07H,ECX=0):EDX[27]=1
#define CPU_MSR_SPEC_CTRL_STIBP_BIT     1

// SSBD If CPUID.(EAX=07H,ECX=0):EDX[31]=1
#define CPU_MSR_SPEC_CTRL_SSBD_BIT      2

#define CPU_MSR_PRED_CMD        0x49

// If CPUID.(EAX=07H,ECX=0):EDX[26]=1
#define CPU_MSR_PRED_CMD_IBPB_BIT   0
#define CPU_MSR_PRED_CMD_IBPB       (1<<CPU_MSR_PRED_CMD_IBPB_BIT)

#define CPU_MSR_SPEC_CTRL_IBRS      (1<<CPU_MSR_SPEC_CTRL_IBRS_BIT)
#define CPU_MSR_SPEC_CTRL_STIBP     (1<<CPU_MSR_SPEC_CTRL_STIBP_BIT)
#define CPU_MSR_SPEC_CTRL_SSBD      (1<<CPU_MSR_SPEC_CTRL_SSBD_BIT)

#define CPU_MSR_SYSENTER_CS     0x174
#define CPU_MSR_SYSENTER_ESP    0x175
#define CPU_MSR_SYSENTER_EIP    0x176

#define CPU_MSR_ARCH_CAPS       0x10A
#define CPU_MSR_ARCH_CAPS_RDCL_NO_BIT   0
#define CPU_MSR_ARCH_CAPS_IBRS_ALL_BIT  1
#define CPU_MSR_ARCH_CAPS_RSBA_BIT      2
#define CPU_MSR_ARCH_CAPS_L1TF_NO_BIT   3
#define CPU_MSR_ARCH_CAPS_SSB_NO_BIT    4
// 1=no rogue cache load
#define CPU_MSR_ARCH_CAPS_RDCL_NO       (1<<CPU_MSR_ARCH_CAPS_RDCL_NO_BIT)
// 1=support IBRS
#define CPU_MSR_ARCH_CAPS_IBRS_ALL      (1<<CPU_MSR_ARCH_CAPS_IBRS_ALL_BIT)
// 1=uses alternate predictor for ret
#define CPU_MSR_ARCH_CAPS_RSBA          (1<<CPU_MSR_ARCH_CAPS_RSBA_BIT)
// 1=no L1 terminal fault
#define CPU_MSR_ARCH_CAPS_L1TF_NO       (1<<CPU_MSR_ARCH_CAPS_L1TF_NO_BIT)
// 1=no speculative store bypass
#define CPU_MSR_ARCH_CAPS_SSB_NO        (1<<CPU_MSR_ARCH_CAPS_SSB_NO_BIT)

#define CPU_MSR_FLUSH_CMD       0x10B
// Writeback and invalidate L1 cache
#define CPU_MSR_FLUSH_CMD_L1D_FLUSH_BIT  0
#define CPU_MSR_FLUSH_CMD_L1D_FLUSH  (1<<CPU_MSR_FLUSH_CMD_L1D_FLUSH_BIT)

// IA32_MISC_ENABLE
#define CPU_MSR_MISC_ENABLE     0x1A0U

// IA32_PERF_BIAS
#define CPU_MSR_PERF_BIAS       0x1B0
//4 bit value, 0=fast, 1=lowpower If CPUID.6H:ECX[3] = 1

// PAT MSR
#define CPU_MSR_IA32_PAT        0x277U

// Uncacheable
#define CPU_MSR_IA32_PAT_UC     0

// Write Combining
#define CPU_MSR_IA32_PAT_WC     1

// Write Through
#define CPU_MSR_IA32_PAT_WT     4

// Write Protected
#define CPU_MSR_IA32_PAT_WP     5

// Writeback
#define CPU_MSR_IA32_PAT_WB     6

// Uncacheable and allow MTRR override
#define CPU_MSR_IA32_PAT_UCW    7

#define CPU_MSR_IA32_PAT_n(n,v) ((uint64_t)(v) << ((n) << 3))

//
// CR0

#define CPU_CR0_PE_BIT 0	// Protected Mode
#define CPU_CR0_MP_BIT 1	// Monitor co-processor
#define CPU_CR0_EM_BIT 2	// Emulation
#define CPU_CR0_TS_BIT 3	// Task switched
#define CPU_CR0_ET_BIT 4	// Extension type
#define CPU_CR0_NE_BIT 5	// Numeric error
#define CPU_CR0_WP_BIT 16	// Write protect
#define CPU_CR0_AM_BIT 18	// Alignment mask
#define CPU_CR0_NW_BIT 29	// Not-write through
#define CPU_CR0_CD_BIT 30	// Cache disable
#define CPU_CR0_PG_BIT 31	// Paging

#define CPU_CR0_PE  (1U << CPU_CR0_PE_BIT)
#define CPU_CR0_MP  (1U << CPU_CR0_MP_BIT)
#define CPU_CR0_EM  (1U << CPU_CR0_EM_BIT)
#define CPU_CR0_TS  (1U << CPU_CR0_TS_BIT)
#define CPU_CR0_ET  (1U << CPU_CR0_ET_BIT)
#define CPU_CR0_NE  (1U << CPU_CR0_NE_BIT)
#define CPU_CR0_WP  (1U << CPU_CR0_WP_BIT)
#define CPU_CR0_AM  (1U << CPU_CR0_AM_BIT)
#define CPU_CR0_NW  (1U << CPU_CR0_NW_BIT)
#define CPU_CR0_CD  (1U << CPU_CR0_CD_BIT)
#define CPU_CR0_PG  (1U << CPU_CR0_PG_BIT)

//
// IA32_MISC_ENABLE

#define CPU_MSR_MISC_ENABLE_FAST_STR_BIT        0
#define CPU_MSR_MISC_ENABLE_AUTO_THERM_BIT      3
#define CPU_MSR_MISC_ENABLE_PERF_MON_BIT        7
#define CPU_MSR_MISC_ENABLE_NO_BTS_BIT          11
#define CPU_MSR_MISC_ENABLE_NO_PEBS_BIT         12
#define CPU_MSR_MISC_ENABLE_ENH_SPEEDSTEP_BIT   16
#define CPU_MSR_MISC_ENABLE_MONITOR_FSM_BIT     18
#define CPU_MSR_MISC_ENABLE_LIMIT_CPUID_BIT     22
#define CPU_MSR_MISC_ENABLE_XTPR_DISABLED_BIT   23
// 33:24 reserved
#define CPU_MSR_MISC_ENABLE_XD_DISABLE_BIT      34

#define CPU_MSR_MISC_ENABLE_FAST_STR \
    (1UL << CPU_MSR_MISC_ENABLE_FAST_STR_BIT)
#define CPU_MSR_MISC_ENABLE_AUTO_THERM \
    (1UL << CPU_MSR_MISC_ENABLE_AUTO_THERM_BIT)
#define CPU_MSR_MISC_ENABLE_PERF_MON \
    (1UL << CPU_MSR_MISC_ENABLE_PERF_MON_BIT)
#define CPU_MSR_MISC_ENABLE_NO_BTS \
    (1UL << CPU_MSR_MISC_ENABLE_NO_BTS_BIT)
#define CPU_MSR_MISC_ENABLE_NO_PEBS \
    (1UL << CPU_MSR_MISC_ENABLE_NO_PEBS_BIT)
#define CPU_MSR_MISC_ENABLE_ENH_SPEEDSTEP \
    (1UL << CPU_MSR_MISC_ENABLE_ENH_SPEEDSTEP_BIT)
#define CPU_MSR_MISC_ENABLE_MONITOR_FSM \
    (1UL << CPU_MSR_MISC_ENABLE_MONITOR_FSM_BIT)
#define CPU_MSR_MISC_ENABLE_LIMIT_CPUID \
    (1UL << CPU_MSR_MISC_ENABLE_LIMIT_CPUID_BIT)
#define CPU_MSR_MISC_ENABLE_XTPR_DISABLED \
    (1UL << CPU_MSR_MISC_ENABLE_XTPR_DISABLED_BIT)
#define CPU_MSR_MISC_ENABLE_XD_DISABLE \
    (1UL << CPU_MSR_MISC_ENABLE_XD_DISABLE_BIT)

//
// CR4

// Virtual 8086 Mode Extensions
#define CPU_CR4_VME_BIT         0

// Protected-mode Virtual Interrupts
#define CPU_CR4_PVI_BIT         1

// Time Stamp Disable
#define CPU_CR4_TSD_BIT         2

// Debugging Extensions
#define CPU_CR4_DE_BIT          3

// Page Size Extension
#define CPU_CR4_PSE_BIT         4

// Physical Address Extension
#define CPU_CR4_PAE_BIT         5

// Machine Check Exception
#define CPU_CR4_MCE_BIT         6

// Page Global Enabled
#define CPU_CR4_PGE_BIT         7

// Performance-Monitoring Counter
#define CPU_CR4_PCE_BIT         8

// OS support for FXSAVE and FXRSTOR
#define CPU_CR4_OFXSR_BIT       9

// OS Support for Unmasked SIMD Floating-Point Exceptions
#define CPU_CR4_OSXMMEX_BIT     10

// (Intel) User mode instruction prevention
#define CPU_CR4_UMIP_BIT        11

// (Intel) Virtual Machine Extensions Enable
#define CPU_CR4_VMXE_BIT        13

// (Intel) Safer Mode Extensions Enable
#define CPU_CR4_SMXE_BIT        14

// Enables instructions RDFSBASE, RDGSBASE, WRFSBASE, and WRGSBASE
#define CPU_CR4_FSGSBASE_BIT    16

// (Intel) PCID Enable
#define CPU_CR4_PCIDE_BIT       17

// XSAVE
#define CPU_CR4_OSXSAVE_BIT     18

// Supervisor Mode Execution Protection Enable
#define CPU_CR4_SMEP_BIT        20

// (Intel) Supervisor Mode Access Protection Enable
#define CPU_CR4_SMAP_BIT        21

// (Intel) Protection Key Enable
#define CPU_CR4_PKE_BIT         22

#define CPU_CR4_VME             (1U << CPU_CR4_VME_BIT     )
#define CPU_CR4_PVI             (1U << CPU_CR4_PVI_BIT     )
#define CPU_CR4_TSD             (1U << CPU_CR4_TSD_BIT     )
#define CPU_CR4_DE              (1U << CPU_CR4_DE_BIT      )
#define CPU_CR4_PSE             (1U << CPU_CR4_PSE_BIT     )
#define CPU_CR4_PAE             (1U << CPU_CR4_PAE_BIT     )
#define CPU_CR4_MCE             (1U << CPU_CR4_MCE_BIT     )
#define CPU_CR4_PGE             (1U << CPU_CR4_PGE_BIT     )
#define CPU_CR4_PCE             (1U << CPU_CR4_PCE_BIT     )
#define CPU_CR4_OFXSR           (1U << CPU_CR4_OFXSR_BIT   )
#define CPU_CR4_OSXMMEX         (1U << CPU_CR4_OSXMMEX_BIT )
#define CPU_CR4_UMIP            (1U << CPU_CR4_UMIP_BIT    )
#define CPU_CR4_VMXE            (1U << CPU_CR4_VMXE_BIT    )
#define CPU_CR4_SMXE            (1U << CPU_CR4_SMXE_BIT    )
#define CPU_CR4_FSGSBASE        (1U << CPU_CR4_FSGSBASE_BIT)
#define CPU_CR4_PCIDE           (1U << CPU_CR4_PCIDE_BIT   )
#define CPU_CR4_OSXSAVE         (1U << CPU_CR4_OSXSAVE_BIT )
#define CPU_CR4_SMEP            (1U << CPU_CR4_SMEP_BIT    )
#define CPU_CR4_SMAP            (1U << CPU_CR4_SMAP_BIT    )
#define CPU_CR4_PKE             (1U << CPU_CR4_PKE_BIT     )

#define CPU_DR7_EN_LOCAL    0x1
#define CPU_DR7_EN_GLOBAL   0x2
#define CPU_DR7_EN_MASK     0x3
#define CPU_DR7_RW_INSN     0x0
#define CPU_DR7_RW_WRITE    0x1
#define CPU_DR7_RW_IO       0x2
#define CPU_DR7_RW_RW       0x3
#define CPU_DR7_RW_MASK     0x3
#define CPU_DR7_LEN_1       0x0
#define CPU_DR7_LEN_2       0x1
#define CPU_DR7_LEN_8       0x2
#define CPU_DR7_LEN_4       0x3
#define CPU_DR7_LEN_MASK    0x3

#define CPU_DR7_BPn_VAL(n, en, rw, len) \
    (((en) << (n * 2)) | \
    ((rw) << ((n) * 4 + 16)) | \
    ((len) << ((n) * 4 + 16 + 2)))

#define CPU_DR7_BPn_MASK(n) \
    CPU_DR7_BPn_VAL((n), CPU_DR7_EN_MASK, CPU_DR7_RW_MASK, CPU_DR7_LEN_MASK)

#define CPU_DR6_BPn_OCCURED(n)  (1<<(n))
#define CPU_DR6_BD_BIT          13
#define CPU_DR6_BS_BIT          14
#define CPU_DR6_BT_BIT          15
#define CPU_DR6_RTM_BIT         16
#define CPU_DR6_BD              (1<<CPU_DR6_BD_BIT)
#define CPU_DR6_BS              (1<<CPU_DR6_BS_BIT)
#define CPU_DR6_BT              (1<<CPU_DR6_BT_BIT)
#define CPU_DR6_RTM             (1<<CPU_DR6_RTM_BIT)

//
// XCR0

// x87 FPU state
#define XCR0_X87_BIT            0

// SSE state
#define XCR0_SSE_BIT            1

// AVX state
#define XCR0_AVX_BIT            2

// Memory Protection BNDREGS
#define XCR0_MPX_BNDREG_BIT     3

// Memory Protection BNDCSR
#define XCR0_MPX_BNDCSR_BIT     4

// AVX-512 opmask registers k0-k7
#define XCR0_AVX512_OPMASK_BIT  5

// AVX-512 upper 256 bits
#define XCR0_AVX512_UPPER_BIT   6

// AVX-512 extra 16 registers
#define XCR0_AVX512_XREGS_BIT   7

// Processor Trace MSRs
#define XCR0_PT_BIT             8

// Protection Key
#define XCR0_PKRU_BIT           9

#define XCR0_X87                (1U<<XCR0_X87_BIT)
#define XCR0_SSE                (1U<<XCR0_SSE_BIT)
#define XCR0_AVX                (1U<<XCR0_AVX_BIT)
#define XCR0_MPX_BNDREG         (1U<<XCR0_MPX_BNDREG_BIT)
#define XCR0_MPX_BNDCSR         (1U<<XCR0_MPX_BNDCSR_BIT)
#define XCR0_AVX512_OPMASK      (1U<<XCR0_AVX512_OPMASK_BIT)
#define XCR0_AVX512_UPPER       (1U<<XCR0_AVX512_UPPER_BIT)
#define XCR0_AVX512_XREGS       (1U<<XCR0_AVX512_XREGS_BIT)
#define XCR0_PT                 (1U<<XCR0_PT_BIT)
#define XCR0_PKRU               (1U<<XCR0_PKRU_BIT)

//
// CPU context

#define CPU_EFLAGS_CF_BIT   0
#define CPU_EFLAGS_PF_BIT   2
#define CPU_EFLAGS_AF_BIT   4
#define CPU_EFLAGS_ZF_BIT   6
#define CPU_EFLAGS_SF_BIT   7
#define CPU_EFLAGS_TF_BIT   8
#define CPU_EFLAGS_IF_BIT   9
#define CPU_EFLAGS_DF_BIT   10
#define CPU_EFLAGS_OF_BIT   11
#define CPU_EFLAGS_IOPL_BIT 12
#define CPU_EFLAGS_NT_BIT   14
#define CPU_EFLAGS_RF_BIT   16
#define CPU_EFLAGS_VM_BIT   17
#define CPU_EFLAGS_AC_BIT   18
#define CPU_EFLAGS_VIF_BIT  19
#define CPU_EFLAGS_VIP_BIT  20
#define CPU_EFLAGS_ID_BIT   21

#define CPU_EFLAGS_CF       (1U << CPU_EFLAGS_CF_BIT)
#define CPU_EFLAGS_PF       (1U << CPU_EFLAGS_PF_BIT)
#define CPU_EFLAGS_AF       (1U << CPU_EFLAGS_AF_BIT)
#define CPU_EFLAGS_ZF       (1U << CPU_EFLAGS_ZF_BIT)
#define CPU_EFLAGS_SF       (1U << CPU_EFLAGS_SF_BIT)
#define CPU_EFLAGS_TF       (1U << CPU_EFLAGS_TF_BIT)
#define CPU_EFLAGS_IF       (1U << CPU_EFLAGS_IF_BIT)
#define CPU_EFLAGS_DF       (1U << CPU_EFLAGS_DF_BIT)
#define CPU_EFLAGS_OF       (1U << CPU_EFLAGS_OF_BIT)
#define CPU_EFLAGS_NT       (1U << CPU_EFLAGS_NT_BIT)
#define CPU_EFLAGS_RF       (1U << CPU_EFLAGS_RF_BIT)
#define CPU_EFLAGS_VM       (1U << CPU_EFLAGS_VM_BIT)
#define CPU_EFLAGS_AC       (1U << CPU_EFLAGS_AC_BIT)
#define CPU_EFLAGS_VIF      (1U << CPU_EFLAGS_VIF_BIT)
#define CPU_EFLAGS_VIP      (1U << CPU_EFLAGS_VIP_BIT)
#define CPU_EFLAGS_ID       (1U << CPU_EFLAGS_ID_BIT)

// Always set
#define CPU_EFLAGS_ALWAYS   2

// Never set (only for debugging)

// Bits 1, 3, 5, 15, and 22 through 31 of eflags are reserved
#define CPU_EFLAGS_NEVER    ((1<<3)|(1<<5)|(1<<15)|-(1<<22))

#define CPU_EFLAGS_IOPL_BITS    2
#define CPU_EFLAGS_IOPL_MASK    ((1 << CPU_EFLAGS_IOPL_BITS)-1)
#define CPU_EFLAGS_IOPL         (CPU_EFLAGS_IOPL_MASK << CPU_EFLAGS_IOPL_BIT)

#define CPU_MXCSR_IE_BIT        0
#define CPU_MXCSR_DE_BIT        1
#define CPU_MXCSR_ZE_BIT        2
#define CPU_MXCSR_OE_BIT        3
#define CPU_MXCSR_UE_BIT        4
#define CPU_MXCSR_PE_BIT        5
#define CPU_MXCSR_DAZ_BIT       6
#define CPU_MXCSR_IM_BIT        7
#define CPU_MXCSR_DM_BIT        8
#define CPU_MXCSR_ZM_BIT        9
#define CPU_MXCSR_OM_BIT        10
#define CPU_MXCSR_UM_BIT        11
#define CPU_MXCSR_PM_BIT        12
#define CPU_MXCSR_RC_BIT        13
#define CPU_MXCSR_FZ_BIT        15

#define CPU_MXCSR_IE            (1U << CPU_MXCSR_IE_BIT)
#define CPU_MXCSR_DE            (1U << CPU_MXCSR_DE_BIT)
#define CPU_MXCSR_ZE            (1U << CPU_MXCSR_ZE_BIT)
#define CPU_MXCSR_OE            (1U << CPU_MXCSR_OE_BIT)
#define CPU_MXCSR_UE            (1U << CPU_MXCSR_UE_BIT)
#define CPU_MXCSR_PE            (1U << CPU_MXCSR_PE_BIT)
#define CPU_MXCSR_DAZ           (1U << CPU_MXCSR_DAZ_BIT)
#define CPU_MXCSR_IM            (1U << CPU_MXCSR_IM_BIT)
#define CPU_MXCSR_DM            (1U << CPU_MXCSR_DM_BIT)
#define CPU_MXCSR_ZM            (1U << CPU_MXCSR_ZM_BIT)
#define CPU_MXCSR_OM            (1U << CPU_MXCSR_OM_BIT)
#define CPU_MXCSR_UM            (1U << CPU_MXCSR_UM_BIT)
#define CPU_MXCSR_PM            (1U << CPU_MXCSR_PM_BIT)
#define CPU_MXCSR_FZ            (1U << CPU_MXCSR_FZ_BIT)

#define CPU_MXCSR_MASK_ALL      (CPU_MXCSR_IM | CPU_MXCSR_DM | CPU_MXCSR_ZM | \
                                CPU_MXCSR_OM | CPU_MXCSR_UM | CPU_MXCSR_PM)

#define CPU_MXCSR_RC_NEAREST    0
#define CPU_MXCSR_RC_DOWN       1
#define CPU_MXCSR_RC_UP         2
#define CPU_MXCSR_RC_TRUNCATE   3

#define CPU_MXCSR_RC_BITS       2
#define CPU_MXCSR_RC_MASK       ((1U << CPU_MXCSR_RC_BITS)-1)
#define CPU_MXCSR_RC            (CPU_MXCSR_RC_MASK << CPU_MXCSR_RC_BIT)
#define CPU_MXCSR_RC_n(rc)      (((rc) & CPU_MXCSR_RC_MASK) << CPU_MXCSR_RC_BIT)

#define CPU_MXCSR_ELF_INIT \
    (CPU_MXCSR_RC_n(CPU_MXCSR_RC_NEAREST) | CPU_MXCSR_MASK_ALL)

//
// Floating point control word

#define CPU_FPUCW_IM_BIT        0
#define CPU_FPUCW_DM_BIT        1
#define CPU_FPUCW_ZM_BIT        2
#define CPU_FPUCW_OM_BIT        3
#define CPU_FPUCW_UM_BIT        4
#define CPU_FPUCW_PM_BIT        5
#define CPU_FPUCW_PC_BIT        8
#define CPU_FPUCW_RC_BIT        10

#define CPU_FPUCW_PC_BITS       2
#define CPU_FPUCW_RC_BITS       2

#define CPU_FPUCW_PC_MASK       ((1U<<CPU_FPUCW_PC_BITS)-1)
#define CPU_FPUCW_RC_MASK       ((1U<<CPU_FPUCW_RC_BITS)-1)

#define CPU_FPUCW_IM            (1U<<CPU_FPUCW_IM_BIT)
#define CPU_FPUCW_DM            (1U<<CPU_FPUCW_DM_BIT)
#define CPU_FPUCW_ZM            (1U<<CPU_FPUCW_ZM_BIT)
#define CPU_FPUCW_OM            (1U<<CPU_FPUCW_OM_BIT)
#define CPU_FPUCW_UM            (1U<<CPU_FPUCW_UM_BIT)
#define CPU_FPUCW_PM            (1U<<CPU_FPUCW_PM_BIT)
#define CPU_FPUCW_PC_n(n)       ((n)<<CPU_FPUCW_PC_BIT)
#define CPU_FPUCW_RC_n(n)       ((n)<<CPU_FPUCW_RC_BIT)

#define CPU_FPUCW_PC_24         0
#define CPU_FPUCW_PC_53         2
#define CPU_FPUCW_PC_64         3

#define CPU_FPUCW_RC_NEAREST    CPU_MXCSR_RC_NEAREST
#define CPU_FPUCW_RC_DOWN       CPU_MXCSR_RC_DOWN
#define CPU_FPUCW_RC_UP         CPU_MXCSR_RC_UP
#define CPU_FPUCW_RC_TRUNCATE   CPU_MXCSR_RC_TRUNCATE

#define CPU_FPUCW_ELF_INIT \
    (CPU_FPUCW_RC_n(CPU_FPUCW_RC_NEAREST) | \
    CPU_FPUCW_PC_n(CPU_FPUCW_PC_53) | \
    CPU_FPUCW_IM | CPU_FPUCW_DM | CPU_FPUCW_ZM | \
    CPU_FPUCW_OM | CPU_FPUCW_UM | CPU_FPUCW_PM)

//
// FPU Status Word

#define CPU_FPUSW_IE_BIT        0
#define CPU_FPUSW_DE_BIT        1
#define CPU_FPUSW_ZE_BIT        2
#define CPU_FPUSW_OE_BIT        3
#define CPU_FPUSW_UE_BIT        4
#define CPU_FPUSW_PE_BIT        5
#define CPU_FPUSW_SF_BIT        6
#define CPU_FPUSW_ES_BIT        7
#define CPU_FPUSW_C0_BIT        8
#define CPU_FPUSW_C1_BIT        9
#define CPU_FPUSW_C2_BIT        10
#define CPU_FPUSW_TOP_BIT       11
#define CPU_FPUSW_C3_BIT        14
#define CPU_FPUSW_B_BIT         15

#define CPU_FPUSW_TOP_BITS      2
#define CPU_FPUSW_TOP_MASK      ((1U<<CPU_FPUSW_TOP_BITS)-1)
#define CPU_FPUSW_TOP_n(n)      ((n)<<CPU_FPUSW_TOP_BIT)


#define CPUID_HIGHESTFUNC       0x0
#define CPUID_INFO_FEATURES     0x1
#define CPUID_CACHE_TLB         0x2
#define CPUID_SERIALNUM         0x3
#define CPUID_TOPOLOGY1         0x4
#define CPUID_MONITOR           0x5
#define CPUID_INFO_EXT_FEATURES 0x7
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


//
// Exception error code

#define CTX_ERRCODE_PF_P_BIT    0
#define CTX_ERRCODE_PF_W_BIT    1
#define CTX_ERRCODE_PF_U_BIT    2
#define CTX_ERRCODE_PF_R_BIT    3
#define CTX_ERRCODE_PF_I_BIT    4
#define CTX_ERRCODE_PF_PK_BIT   5
#define CTX_ERRCODE_PF_SGX_BIT  15

// Page fault because page not present
#define CTX_ERRCODE_PF_P        (1<<CTX_ERRCODE_PF_P_BIT)

// Page fault was a write
#define CTX_ERRCODE_PF_W        (1<<CTX_ERRCODE_PF_W_BIT)

// Page fault occurred in user mode
#define CTX_ERRCODE_PF_U        (1<<CTX_ERRCODE_PF_U_BIT)

// Page fault because reserved PTE field was 1
#define CTX_ERRCODE_PF_R        (1<<CTX_ERRCODE_PF_R_BIT)

// Page fault was instruction fetch
#define CTX_ERRCODE_PF_I        (1<<CTX_ERRCODE_PF_I_BIT)

// Page fault was protection keys violation
#define CTX_ERRCODE_PF_PK       (1<<CTX_ERRCODE_PF_PK_BIT)

// Page fault was instruction fetch
#define CTX_ERRCODE_PF_SGX      (1<<CTX_ERRCODE_PF_SGX_BIT)

#define GDT_SEL_PM_CODE16       0x08
#define GDT_SEL_PM_DATA16       0x10
#define GDT_SEL_PM_CODE32       0x18
#define GDT_SEL_PM_DATA32       0x20

#define GDT_SEL_USER_CODE32     0x40
#define GDT_SEL_USER_DATA       0x48
#define GDT_SEL_USER_CODE64     0x50

#define GDT_SEL_KERNEL_CODE64   0x60
#define GDT_SEL_KERNEL_DATA     0x68

#define GDT_SEL_TSS             0x80
#define GDT_SEL_TSS_HI          0x88

#define GDT_SEL_END             0xC0

#define GDT_SEL_RPL_OF(sel)     ((sel) & 3)

// Selector is a 32 or 64 bit kernel or user code segment
#define GDT_SEL_IS_CSEG(sel)    (((sel) >= 0x40+3) && \
                                    ((sel) <= 0x60) && \
                                    !((sel) & 8))

// Selector is a kernel or user data segment
#define GDT_SEL_IS_DSEG(sel)    (((sel) >= 0x48+3) && \
                                    ((sel) <= 0x68) && \
                                    ((sel) & 8))

#define GDT_SEL_IS_C64(sel)     (((sel) >= 0x50+3) && \
                                    ((sel) <= 0x60) && \
                                    !((sel) & 8))

#define GDT_SEL_RPL_IS_KERNEL(sel)  (GDT_SEL_RPL_OF((sel)) == 0)
#define GDT_SEL_RPL_IS_USER(sel)    (GDT_SEL_RPL_OF((sel)) == 3)

#define GDT_TYPE_TSS            0x09

#define GDT_ACCESS_PRESENT_BIT  7
#define GDT_ACCESS_DPL_BIT      5
#define GDT_ACCESS_EXEC_BIT     3
#define GDT_ACCESS_DOWN_BIT     2
#define GDT_ACCESS_RW_BIT       1
#define GDT_ACCESS_ACCESSED_BIT 0

#define GDT_ACCESS_PRESENT      (1 << GDT_ACCESS_PRESENT_BIT)
#define GDT_ACCESS_EXEC         (1 << GDT_ACCESS_EXEC_BIT)
#define GDT_ACCESS_DOWN         (1 << GDT_ACCESS_DOWN_BIT)
#define GDT_ACCESS_RW           (1 << GDT_ACCESS_RW_BIT)
#define GDT_ACCESS_ACCESSED     (1 << GDT_ACCESS_ACCESSED_BIT)

#define GDT_ACCESS_DPL_BITS     2
#define GDT_ACCESS_DPL_MASK     ((1 << GDT_ACCESS_DPL_BITS)-1)
#define GDT_ACCESS_DPL          (GDT_ACCESS_DPL_MASK << GDT_ACCESS_DPL_BIT)
#define GDT_ACCESS_DPL_n(dpl)   ((dpl) << GDT_ACCESS_DPL_BIT)

#define GDT_FLAGS_GRAN_BIT      7
#define GDT_FLAGS_IS32_BIT      6
#define GDT_FLAGS_IS64_BIT      5

#define GDT_FLAGS_GRAN          (1 << GDT_FLAGS_GRAN_BIT)
#define GDT_FLAGS_IS32          (1 << GDT_FLAGS_IS32_BIT)
#define GDT_FLAGS_IS64          (1 << GDT_FLAGS_IS64_BIT)

#define GDT_LIMIT_LOW_MASK      0xFFFF
#define GDT_BASE_LOW_MASK       0xFFFF

#define GDT_BASE_MIDDLE_BIT     16
#define GDT_BASE_MIDDLE         0xFF

#define GDT_LIMIT_HIGH_BIT      16
#define GDT_LIMIT_HIGH          0x0F

#define GDT_BASE_HIGH_BIT       24
#define GDT_BASE_HIGH           0xFF

// idt_entry_t selector field
#define IDT_SEL         GDT_SEL_KERNEL_CODE64

// idt_entry_t type_attr field
#define IDT_PRESENT_BIT 7
#define IDT_DPL_BIT     5
#define IDT_TYPE_BIT    0

#define IDT_TYPE_TASK   0x5
#define IDT_TYPE_INTR   0xE
#define IDT_TYPE_TRAP   0xF

#define IDT_PRESENT     (1 << IDT_PRESENT_BIT)

#define IDT_DPL_BITS    2
#define IDT_DPL_MASK    ((1 << IDT_DPL_BITS)-1)
#define IDT_DPL         (IDT_DPL_MASK << IDT_DPL_BIT)

#define IDT_DPL_n(dpl)  (((dpl) & IDT_DPL_MASK) << IDT_DPL_BIT)

#define IDT_TASK        (IDT_TYPE_TASK << IDT_TYPE_BIT)
#define IDT_INTR        (IDT_TYPE_INTR << IDT_TYPE_BIT)
#define IDT_TRAP        (IDT_TYPE_TRAP << IDT_TYPE_BIT)

//
// APIC_BASE MSR

#define CPU_APIC_BASE_MSR           0x1B

#define CPU_APIC_BASE_ADDR_BIT      12
#define CPU_APIC_BASE_ADDR_BITS     40
#define CPU_APIC_BASE_GENABLE_BIT   11
#define CPU_APIC_BASE_X2ENABLE_BIT  10
#define CPU_APIC_BASE_BSP_BIT       8

#define CPU_APIC_BASE_GENABLE       (1UL<<CPU_APIC_BASE_GENABLE_BIT)
#define CPU_APIC_BASE_X2ENABLE      (1UL<<CPU_APIC_BASE_X2ENABLE_BIT)
#define CPU_APIC_BASE_BSP           (1UL<<CPU_APIC_BASE_BSP_BIT)
#define CPU_APIC_BASE_ADDR_MASK     ((1UL<<CPU_APIC_BASE_ADDR_BITS)-1)
#define CPU_APIC_BASE_ADDR          \
    (CPU_APIC_BASE_ADDR_MASK<<CPU_APIC_BASE_ADDR_BIT)
