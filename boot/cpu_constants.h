#pragma once

// CPU constants (safe to include in assembly)


// IA32_EFER MSR
#define CPU_MSR_EFER            0xC0000080U

#define CPU_MSR_EFER_LME_BIT    8
#define CPU_MSR_EFER_NX_BIT     11

#define CPU_MSR_EFER_LME        (1U << CPU_MSR_EFER_LME_BIT)
#define CPU_MSR_EFER_NX         (1U << CPU_MSR_EFER_NX_BIT)

// IA32_XSS MSR
#define CPU_MSR_IA32_XSS        0xDA0

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

//
// CR0

#define CPU_CR0_PE_BIT 0	// Protected Mode
#define CPU_CR0_MP_BIT 1	// Monitor co-processor
#define CPU_CR0_EM_BIT 2	// Emulation
#define CPU_CR0_TS_BIT 3	// Task switched
#define CPU_CR0_ET_BIT 4	// Extension type (0=80287)
#define CPU_CR0_NE_BIT 5	// Numeric error (1=exception, 0=IRQ)
#define CPU_CR0_WP_BIT 16	// Write protect (1=enforce in ring0)
#define CPU_CR0_AM_BIT 18	// Alignment mask (1=enable EFLAGS.AC)
#define CPU_CR0_NW_BIT 29	// Not-write through
#define CPU_CR0_CD_BIT 30	// Cache disable
#define CPU_CR0_PG_BIT 31	// Paging (1=enable paging)

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
// CR4

#define CPU_CR4_VME_BIT         0	// Virtual 8086 Mode Extensions
#define CPU_CR4_PVI_BIT         1	// Protected-mode Virtual Interrupts
#define CPU_CR4_TSD_BIT         2	// Time Stamp Disable
#define CPU_CR4_DE_BIT          3	// Debugging Extensions
#define CPU_CR4_PSE_BIT         4	// Page Size Extension
#define CPU_CR4_PAE_BIT         5	// Physical Address Extension
#define CPU_CR4_MCE_BIT         6	// Machine Check Exception
#define CPU_CR4_PGE_BIT         7	// Page Global Enabled
#define CPU_CR4_PCE_BIT         8	// Performance-Monitoring Counter
#define CPU_CR4_OFXSR_BIT       9	// Support FXSAVE and FXRSTOR
#define CPU_CR4_OSXMMEX_BIT     10	// Support Unmasked SIMD FP Exceptions
#define CPU_CR4_VMXE_BIT        13	// Virtual Machine Extensions Enable
#define CPU_CR4_SMXE_BIT        14	// Safer Mode Extensions Enable
#define CPU_CR4_FSGSBASE_BIT    16	// Enables instructions {RD,WR}{FS,GS}BASE
#define CPU_CR4_PCIDE_BIT       17	// PCID Enable
#define CPU_CR4_OSXSAVE_BIT     18	// XSAVE
#define CPU_CR4_SMEP_BIT        20	// Supervisor Mode Execution Protect Enable
#define CPU_CR4_SMAP_BIT        21	// Supervisor Mode Access Protect Enable
#define CPU_CR4_PKE_BIT         22	// Protection Key Enable

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
#define CPU_CR4_VMXE            (1U << CPU_CR4_VMXE_BIT    )
#define CPU_CR4_SMXE            (1U << CPU_CR4_SMXE_BIT    )
#define CPU_CR4_FSGSBASE        (1U << CPU_CR4_FSGSBASE_BIT)
#define CPU_CR4_PCIDE           (1U << CPU_CR4_PCIDE_BIT   )
#define CPU_CR4_OSXSAVE         (1U << CPU_CR4_OSXSAVE_BIT )
#define CPU_CR4_SMEP            (1U << CPU_CR4_SMEP_BIT    )
#define CPU_CR4_SMAP            (1U << CPU_CR4_SMAP_BIT    )
#define CPU_CR4_PKE             (1U << CPU_CR4_PKE_BIT     )

#define GDT_ACCESS_PRESENT_BIT  7
#define GDT_ACCESS_DPL_BIT      5
#define GDT_ACCESS_EXEC_BIT     3
#define GDT_ACCESS_DOWN_BIT     2
#define GDT_ACCESS_RW_BIT       1
#define GDT_ACCESS_ACCESSED_BIT 0

#define GDT_ACCESS_PRESENT      (1U << GDT_ACCESS_PRESENT_BIT)
#define GDT_ACCESS_EXEC         (1U << GDT_ACCESS_EXEC_BIT)
#define GDT_ACCESS_DOWN         (1U << GDT_ACCESS_DOWN_BIT)
#define GDT_ACCESS_RW           (1U << GDT_ACCESS_RW_BIT)
#define GDT_ACCESS_ACCESSED     (1U << GDT_ACCESS_ACCESSED_BIT)

#define GDT_ACCESS_DPL_BITS     2
#define GDT_ACCESS_DPL_MASK     ((1 << GDT_ACCESS_DPL_BITS)-1)
#define GDT_ACCESS_DPL          (GDT_ACCESS_DPL_MASK << GDT_ACCESS_DPL_BIT)
#define GDT_ACCESS_DPL_n(dpl)   ((dpl) << GDT_ACCESS_DPL_BIT)

#define GDT_FLAGS_GRAN_BIT      7
#define GDT_FLAGS_IS32_BIT      6
#define GDT_FLAGS_IS64_BIT      5

#define GDT_FLAGS_GRAN          (1U << GDT_FLAGS_GRAN_BIT)
#define GDT_FLAGS_IS32          (1U << GDT_FLAGS_IS32_BIT)
#define GDT_FLAGS_IS64          (1U << GDT_FLAGS_IS64_BIT)

#define GDT_LIMIT_LOW_MASK      0xFFFF
#define GDT_BASE_LOW_MASK       0xFFFF

#define GDT_BASE_MIDDLE_BIT     16
#define GDT_BASE_MIDDLE         0xFF

#define GDT_LIMIT_HIGH_BIT      16
#define GDT_LIMIT_HIGH_MASK     0x0F

#define GDT_BASE_HIGH_BIT       24
#define GDT_BASE_HIGH           0xFF

// idt_entry_t type_attr field
#define IDT_PRESENT     0x80
#define IDT_DPL_BIT     5
#define IDT_DPL_BITS    2
#define IDT_DPL_MASK    ((1 << IDT_DPL_BITS)-1)
#define IDT_DPL3        (3 << IDT_DPL_BIT)
#define IDT_TASK        0x05
#define IDT_INTR        0x0E
#define IDT_TRAP        0x0F
