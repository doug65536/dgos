#include "dev_storage.h"
#include "ata.h"
#include "device/pci.h"
#include "irq.h"
#include "printk.h"
#include "mm.h"
#include "bitsearch.h"
#include "assert.h"
#include "thread.h"
#include "string.h"
#include "cpu/atomic.h"
#include "time.h"
#include "cpu/spinlock.h"
#include "threadsync.h"
#include "cpu/control_regs.h"
#include "unique_ptr.h"

#define AHCI_DEBUG  1
#if AHCI_DEBUG
#define AHCI_TRACE(...) printdbg("ahci: " __VA_ARGS__)
#else
#define AHCI_TRACE(...) ((void)0)
#endif

enum ahci_fis_type_t {
    // Register FIS - host to device
    FIS_TYPE_REG_H2D    = 0x27,

    // Register FIS - device to host
    FIS_TYPE_REG_D2H    = 0x34,

    // DMA activate FIS - device to host
    FIS_TYPE_DMA_ACT    = 0x39,

    // DMA setup FIS - bidirectional
    FIS_TYPE_DMA_SETUP  = 0x41,

    // Data FIS - bidirectional
    FIS_TYPE_DATA       = 0x46,

    // BIST activate FIS - bidirectional
    FIS_TYPE_BIST       = 0x58,

    // PIO setup FIS - device to host
    FIS_TYPE_PIO_SETUP  = 0x5F,

    // Set device bits FIS - device to host
    FIS_TYPE_DEV_BITS   = 0xA1,
};

// Host-to-Device
struct ahci_fis_h2d_t {
    // FIS_TYPE_REG_H2D
    uint8_t fis_type;

    // PORTMUX and CMD
    uint8_t ctl;

    // Command register
    ata_cmd_t command;

    // Feature register, 7:0
    uint8_t feature_lo;


    // LBA low register, 7:0
    uint8_t lba0;

    // LBA mid register, 15:8
    uint8_t lba1;

    // LBA high register, 23:16
    uint8_t lba2;

    // Device register
    uint8_t device;


    // LBA register, 31:24
    uint8_t lba3;

    // LBA register, 39:32
    uint8_t lba4;

    // LBA register, 47:40
    uint8_t lba5;

    // Feature register, 15:8
    uint8_t feature_hi;

    // Count register, 15:0
    uint16_t count;

    // Isochronous command completion
    uint8_t icc;

    // Control register
    uint8_t control;

    // Reserved
    uint8_t rsv1[4];

    inline void set_lba(uint64_t lba)
    {
        assert(lba < 0x1000000000000);
        lba0 = (lba >> (0*8)) & 0xFF;
        lba1 = (lba >> (1*8)) & 0xFF;
        lba2 = (lba >> (2*8)) & 0xFF;
        lba3 = (lba >> (3*8)) & 0xFF;
        lba4 = (lba >> (4*8)) & 0xFF;
        lba5 = (lba >> (5*8)) & 0xFF;
    }

    inline void set_count(uint32_t new_count)
    {
        assert(new_count <= 0x10000);
        count = new_count;
    }
};

C_ASSERT(sizeof(ahci_fis_h2d_t) == 20);

#define AHCI_FIS_CTL_PORTMUX_BIT    0
#define AHCI_FIS_CTL_PORTMUX_BITS   4
#define AHCI_FIS_CTL_CMD_BIT        7

#define AHCI_FIS_CTL_PORTMUX_MASK   ((1U<<AHCI_FIS_CTL_PORTMUX_BITS)-1)
#define AHCI_FIS_CTL_PORTMUX_n(n)   ((n)<<AHCI_FIS_CTL_PORTMUX_BITS)

#define AHCI_FIS_CTL_CMD            (1U<<AHCI_FIS_CTL_CMD_BIT)
#define AHCI_FIS_CTL_CTL            (0U<<AHCI_FIS_CTL_CMD_BIT)

// Native Command Queuing
struct ahci_fis_ncq_t {
    // FIS_TYPE_REG_H2D
    uint8_t fis_type;

    // PORTMUX and CMD
    uint8_t ctl;

    // Command register
    ata_cmd_t command;

    // Count register, 7:0
    uint8_t count_lo;

    // LBA low register, 7:0
    uint8_t lba0;

    // LBA mid register, 15:8
    uint8_t lba1;

    // LBA high register, 23:16
    uint8_t lba2;

    // Force Unit Access (bit 7)
    uint8_t fua;

    // LBA register, 31:24
    uint8_t lba3;

    // LBA register, 39:32
    uint8_t lba4;

    // LBA register, 47:40
    uint8_t lba5;

    // Count register, 15:8
    uint8_t count_hi;

    // Tag register 7:3, RARC bit 0
    uint8_t tag;

    // Priority register (7:6)
    uint8_t prio;

    // Isochronous command completion
    uint8_t icc;

    // Control register
    uint8_t control;

    // Auxiliary
    uint32_t aux;

    inline void set_lba(uint64_t lba)
    {
        assert(lba < 0x1000000000000);
        lba0 = (lba >> (0*8)) & 0xFFU;
        lba1 = (lba >> (1*8)) & 0xFFU;
        lba2 = (lba >> (2*8)) & 0xFFU;
        lba3 = (lba >> (3*8)) & 0xFFU;
        lba4 = (lba >> (4*8)) & 0xFFU;
        lba5 = (lba >> (5*8)) & 0xFFU;
    }

    inline void set_count(uint32_t count)
    {
        assert(count <= 0x10000);
        count_lo = (count >> (0*8)) & 0xFFU;
        count_hi = (count >> (1*8)) & 0xFFU;
    }
};

//
// ahci_fis_ncq_t::tag

// NCQ tag
#define AHCI_FIS_TAG_TAG_BIT    3
#define AHCI_FIS_TAG_TAG_BITS   5

// Rebuild Assist
#define AHCI_FIS_TAG_RARC_BIT   0

#define AHCI_FIS_TAG_TAG_n(n)   ((n)<<AHCI_FIS_TAG_TAG_BIT)
#define AHCI_FIS_TAG_TAG_MASK   ((1U<<AHCI_FIS_TAG_TAG_BITS)-1)
#define AHCI_FIS_TAG_TAG        (AHCI_FIS_TAG_TAG_MASK<<AHCI_FIS_TAG_TAG_BIT)

#define AHCI_FIS_TAG_RARC       (1U<<AHCI_FIS_TAG_RARC_BIT)

//
// ahci_fis_ncq_t::prio

#define AHCI_FIS_PRIO_BIT       6
#define AHCI_FIS_PRIO_BITS      2
#define AHCI_FIS_PRIO_MASK      ((1U<<AHCI_FIS_PRIO_BIT)-1)
#define AHCI_FIS_PRIO_MASK_n(n) ((n)<<AHCI_FIS_PRIO_BIT)
#define AHCI_FIS_PRIO           (AHCI_FIS_PRIO_MASK<<AHCI_FIS_PRIO_BIT)

//
// ahci_fis_ncq_t::fua

#define AHCI_FIS_FUA_FUA_BIT    7
#define AHCI_FIS_FUA_LBA_BIT    6

#define AHCI_FIS_FUA_FUA        (1U<<AHCI_FIS_FUA_FUA_BIT)
#define AHCI_FIS_FUA_LBA        (1U<<AHCI_FIS_FUA_LBA_BIT)

struct ahci_fis_d2h_t {
    // FIS_TYPE_REG_D2H
    uint8_t fis_type;

    // PORTMUX and INTR
    uint8_t ctl;

    // Status
    uint8_t command;

    // Error
    uint8_t error;

    // LBA 15:0
    uint16_t lba0;

    // LBA 23:16
    uint8_t lba2;

    // Device
    uint8_t	device;

    // LBA 39:24
    uint16_t lba3;

    // LBA 47:40
    uint8_t lba5;

    // Reserved
    uint8_t rsv2;

    // Count 15:0
    uint16_t count;

    uint8_t rsv3[2];    // Reserved

    uint32_t rsv4;      // Reserved
};

#define AHCI_FIS_CTL_INTR_BIT   6
#define AHCI_FIS_CTL_INTR       (1U<<AHCI_FIS_CTL_INTR_BIT)

struct ahci_pio_setup_t {
    // FIS_TYPE_PIO_SETUP
    uint8_t	fis_type;

    // INTR, DIR
    uint8_t	ctl;

    // Status register
    uint8_t status;

    // Error register
    uint8_t error;

    // LBA 15:0
    uint16_t lba0;
    // LBA high register, 23:16
    uint8_t lba2;
    // Device register
    uint8_t device;

    // LBA 39:24
    uint16_t lba3;
    // LBA register, 47:40
    uint8_t lba5;
    // Reserved
    uint8_t rsv2;

    // Count register, 15:0
    uint16_t count;
    // Reserved
    uint8_t rsv3;
    // New value of status register
    uint8_t e_status;

    // Transfer count
    uint16_t tc;
    // Reserved
    uint16_t rsv4;
};

struct ahci_dma_setup_t {
    // FIS_TYPE_DMA_SETUP
    uint8_t	fis_type;

    // PORTMUX, DIR, INTR, AUTO
    uint8_t	ctl;

    // Reserved
    uint16_t rsv;

    // DMA Buffer Identifier. Used to Identify DMA buffer in host memory.
    // SATA Spec says host specific and not in Spec.
    // Trying AHCI spec might work.
    uint32_t dma_buf_id[2];

    // Reserved
    uint32_t rsv2;

    // Byte offset into buffer. First 2 bits must be 0
    uint32_t dma_buf_ofs;

    //Number of bytes to transfer. Bit 0 must be 0
    uint32_t transfer_count;

    //Reserved
    uint32_t rsv3;
};

C_ASSERT(sizeof(ahci_dma_setup_t) == 0x1C);

#define AHCI_FIS_CTL_DIR_BIT    5
#define AHCI_FIS_CTL_AUTO_BIT   7

#define AHCI_FIS_CTL_DIR        (1U<<AHCI_FIS_CTL_DIR_BIT)
#define AHCI_FIS_AUTO_DIR       (1U<<AHCI_FIS_CTL_AUTO_BIT)

// MMIO

enum struct ahci_sig_t : uint32_t {
    // SATA drive
    SATA_SIG_ATA    = 0x00000101,

    // SATAPI drive
    SATA_SIG_ATAPI  = 0xEB140101,

    // Enclosure management bridge
    SATA_SIG_SEMB   = 0xC33C0101,

    // Port multiplier
    SATA_SIG_PM     = 0x96690101
};

C_ASSERT(sizeof(ahci_sig_t) == 4);

struct hba_port_t {
    // command list base address, 1K-byte aligned
    uint64_t cmd_list_base;

    // FIS base address,
    //  When FIS-based switching off:
    //   256-byte aligned, 256-bytes size
    //  When FIS-based switching on:
    //   4KB aligned, 4KB size
    uint64_t fis_base;

    // interrupt status
    uint32_t intr_status;

    // interrupt enable
    uint32_t intr_en;

    // command and status
    uint32_t cmd;

    // Reserved
    uint32_t rsv0;

    // Task file data
    uint32_t taskfile_data;

    // Signature
    ahci_sig_t sig;

    // SATA status (SCR0:SStatus)
    uint32_t sata_status;

    // SATA control (SCR2:SControl)
    uint32_t sata_ctl;

    // SATA error (SCR1:SError)
    uint32_t sata_err;

    // SATA active (SCR3:SActive) (bitmask by port)
    uint32_t sata_act;

    // command issue (bitmask by port)
    uint32_t cmd_issue;

    // SATA notification (SCR4:SNotification)
    uint32_t sata_notify;

    // FIS-based switch control
    uint32_t fis_based_sw;

    // Reserved
    uint32_t rsv1[11];

    // Vendor specific
    uint32_t vendor[4];
};

C_ASSERT(offsetof(hba_port_t, sata_act) == 0x34);
C_ASSERT(sizeof(hba_port_t) == 0x80);

//
// hba_port_t::sata_status
#define AHCI_SS_DET_BIT         0
#define AHCI_SS_DET_BITS        4
#define AHCI_SS_DET_MASK        ((1U<<AHCI_SS_DET_BITS)-1)
#define AHCI_SS_DET             (AHCI_SS_DET_MASK<<AHCI_SS_DET_BIT)
#define AHCI_SS_DET_n(n)        ((n)<<AHCI_SS_DET_BIT)

#define AHCI_SS_DET_NODEV       0
#define AHCI_SS_DET_NOPHY       1
#define AHCI_SS_DET_ONLINE      3
#define AHCI_SS_DET_OFFLINE     4

#define AHCI_SS_SPD_BIT         4
#define AHCI_SS_SPD_BITS        4
#define AHCI_SS_SPD_MASK        ((1U<<AHCI_SS_SPD_BITS)-1)
#define AHCI_SS_SPD             (AHCI_SS_SPD_MASK<<AHCI_SS_SPD_BIT)
#define AHCI_SS_SPD_n(n)        ((n)<<AHCI_SS_SPD_BIT)

#define AHCI_SS_SPD_NODEV       0
#define AHCI_SS_SPD_GEN1        1
#define AHCI_SS_SPD_GEN2        2
#define AHCI_SS_SPD_GEN3        3
#define AHCI_SS_SPD_GEN_n(n)    (n)

#define AHCI_SS_IPM_BIT         8
#define AHCI_SS_IPM_BITS        4
#define AHCI_SS_IPM_MASK        ((1U<<AHCI_SS_IPM_BITS)-1)
#define AHCI_SS_IPM             (AHCI_SS_IPM_MASK<<AHCI_SS_IPM_BIT)
#define AHCI_SS_IPM_n(n)        ((n)<<AHCI_SS_IPM_BIT)

#define AHCI_SS_IPM_NODEV       0
#define AHCI_SS_IPM_ACTIVE      1
#define AHCI_SS_IPM_PARTIAL     2
#define AHCI_SS_IPM_SLUMBER     6
#define AHCI_SS_IPM_DEVSLEEP    8

// 0x00 - 0x2B, Generic Host Control
struct hba_host_ctl_t {
    // Host capability
    uint32_t cap;

    // Global host control
    uint32_t host_ctl;

    // Interrupt status (bitmask by port)
    uint32_t intr_status;

    // Port implemented (bitmask, 1=implemented)
    uint32_t ports_impl_mask;

    // Version
    uint32_t version;

    // Command completion coalescing control
    uint32_t ccc_ctl;

    // Command completion coalescing ports
    uint32_t ccc_pts;

    // Enclosure management location
    uint32_t encl_mgmt_loc;

    // Enclosure management control
    uint32_t em_ctl;

    // Host capabilities extended
    uint32_t cap2;

    // BIOS/OS handoff control and status
    uint32_t bios_handoff;

    // 0x2C - 0x9F, Reserved
    uint8_t rsv[0xA0 - 0x2C];

    // 0xA0 - 0xFF, Vendor specific registers
    uint8_t vendor[0x100 - 0xA0];

    // 0x100 - 0x10FF, Port control registers
    hba_port_t ports[32];	// 1 ~ 32
};

C_ASSERT(offsetof(hba_host_ctl_t, rsv) == 0x2C);
C_ASSERT(offsetof(hba_host_ctl_t, ports) == 0x100);

//
// hba_host_ctl::cap
#define AHCI_HC_CAP_S64A_BIT    31
#define AHCI_HC_CAP_SNCQ_BIT    30
#define AHCI_HC_CAP_SSNTF_BIT   29
#define AHCI_HC_CAP_SMPS_BIT    28
#define AHCI_HC_CAP_SSS_BIT     27
#define AHCI_HC_CAP_SALP_BIT    26
#define AHCI_HC_CAP_SAL_BIT     25
#define AHCI_HC_CAP_SCLO_BIT    24
#define AHCI_HC_CAP_ISS_BIT     20
#define AHCI_HC_CAP_SAM_BIT     18
#define AHCI_HC_CAP_SPM_BIT     17
#define AHCI_HC_CAP_FBSS_BIT    16
#define AHCI_HC_CAP_PMD_BIT     15
#define AHCI_HC_CAP_SSC_BIT     14
#define AHCI_HC_CAP_PSC_BIT     13
#define AHCI_HC_CAP_NCS_BIT     8
#define AHCI_HC_CAP_CCCS_BIT    7
#define AHCI_HC_CAP_EMS_BIT     6
#define AHCI_HC_CAP_SXS_BIT     5
#define AHCI_HC_CAP_NP_BIT      0

#define AHCI_HC_CAP_NCS_BITS    5
#define AHCI_HC_CAP_ISS_BITS    4
#define AHCI_HC_CAP_NP_BITS     5

// 64 bit capable
#define AHCI_HC_CAP_S64A        (1U<<AHCI_HC_CAP_S64A_BIT)

// Supports Native Command Queuing
#define AHCI_HC_CAP_SNCQ        (1U<<AHCI_HC_CAP_SNCQ_BIT)

// Supports SNotification Register
#define AHCI_HC_CAP_SSNTF       (1U<<AHCI_HC_CAP_SSNTF_BIT)

// Supports mechanical presence switch
#define AHCI_HC_CAP_SMPS        (1U<<AHCI_HC_CAP_SMPS_BIT)

// Supports Staggered Spinup
#define AHCI_HC_CAP_SSS         (1U<<AHCI_HC_CAP_SSS_BIT)

// Supports Aggressive Link Power management
#define AHCI_HC_CAP_SALP        (1U<<AHCI_HC_CAP_SALP_BIT)

// Supports Activity LED
#define AHCI_HC_CAP_SAL         (1U<<AHCI_HC_CAP_SAL_BIT)

// Supports Command List Override
#define AHCI_HC_CAP_SCLO        (1U<<AHCI_HC_CAP_SCLO_BIT)

// Interface speed support
#define AHCI_HC_CAP_ISS         (1U<<AHCI_HC_CAP_ISS_BIT)

// Supports AHCI Mode only
#define AHCI_HC_CAP_SAM         (1U<<AHCI_HC_CAP_SAM_BIT)

// Supports Port Multiplier
#define AHCI_HC_CAP_SPM         (1U<<AHCI_HC_CAP_SPM_BIT)

// FIS Based Switching Supported
#define AHCI_HC_CAP_FBSS        (1U<<AHCI_HC_CAP_FBSS_BIT)

// PIO Multiple DRQ Block
#define AHCI_HC_CAP_PMD         (1U<<AHCI_HC_CAP_PMD_BIT)

// Slumber State Capable
#define AHCI_HC_CAP_SSC         (1U<<AHCI_HC_CAP_SSC_BIT)

// Partial State Capable
#define AHCI_HC_CAP_PSC         (1U<<AHCI_HC_CAP_PSC_BIT)

// Number of Command Slots
#define AHCI_HC_CAP_NCS         (1U<<AHCI_HC_CAP_NCS_BIT)

// Command Completetion Coalescing Supported
#define AHCI_HC_CAP_CCCS        (1U<<AHCI_HC_CAP_CCCS_BIT)

// Enclosure Management Supported
#define AHCI_HC_CAP_EMS         (1U<<AHCI_HC_CAP_EMS_BIT)

// Supports eXternal SATA
#define AHCI_HC_CAP_SXS         (1U<<AHCI_HC_CAP_SXS_BIT)

// Number of Ports
#define AHCI_HC_CAP_NP          (1U<<AHCI_HC_CAP_NP_BIT)

#define AHCI_HC_CAP_ISS_MASK    ((1U<<AHCI_HC_CAP_ISS_BITS)-1)
#define AHCI_HC_CAP_NCS_MASK    ((1U<<AHCI_HC_CAP_NCS_BITS)-1)
#define AHCI_HC_CAP_NP_MASK     ((1U<<AHCI_HC_CAP_NP_BITS)-1)

//
// hba_host_ctl::cap2
#define AHCI_HC_CAP2_DESO_BIT   5
#define AHCI_HC_CAP2_SADM_BIT   4
#define AHCI_HC_CAP2_SDS_BIT    3
#define AHCI_HC_CAP2_APST_BIT   2
#define AHCI_HC_CAP2_NVMP_BIT   1
#define AHCI_HC_CAP2_BOH_BIT    0

// DevSleep Entrance From Slumber Only
#define AHCI_HC_CAP2_DESO       (1U<<AHCI_HC_CAP2_DESO_BIT)

// Supports Aggressive Device sleep Management
#define AHCI_HC_CAP2_SADM       (1U<<AHCI_HC_CAP2_SADM_BIT)

// Supports Device Sleep
#define AHCI_HC_CAP2_SDS        (1U<<AHCI_HC_CAP2_SDS_BIT)

// Automatic Partial Slumber Transitions
#define AHCI_HC_CAP2_APST       (1U<<AHCI_HC_CAP2_APST_BIT)

// NVMHCI Present
#define AHCI_HC_CAP2_NVMP       (1U<<AHCI_HC_CAP2_NVMP_BIT)

// BIOS/OS Handoff
#define AHCI_HC_CAP2_BOH        (1U<<AHCI_HC_CAP2_BOH_BIT)

//
// hba_host_ctl::bios_handoff
#define AHCI_HC_BOH_BB_BIT      4
#define AHCI_HC_BOH_OOC_BIT     3
#define AHCI_HC_BOH_SOOE_BIT    2
#define AHCI_HC_BOH_OOS_BIT     1
#define AHCI_HC_BOH_BOS_BIT     0

// BIOS Busy
#define AHCI_HC_BOH_BB          (1U<<AHCI_HC_BOH_BB_BIT)

// OS Ownership Change
#define AHCI_HC_BOH_OOC         (1U<<AHCI_HC_BOH_OOC_BIT)

// SMI on OS Ownership Change
#define AHCI_HC_BOH_SOOE        (1U<<AHCI_HC_BOH_SOOE_BIT)

// OS Owned Semaphore
#define AHCI_HC_BOH_OOS         (1U<<AHCI_HC_BOH_OOS_BIT)

// BIOS Owned Semaphore
#define AHCI_HC_BOH_BOS         (1U<<AHCI_HC_BOH_BOS_BIT)

//
// hba_host_ctl::host_ctl
#define AHCI_HC_HC_AE_BIT       31
#define AHCI_HC_HC_MRSM_BIT     2
#define AHCI_HC_HC_IE_BIT       1
#define AHCI_HC_HC_HR_BIT       0

// AHCI Enable
#define AHCI_HC_HC_AE           (1U<<AHCI_HC_HC_AE_BIT)

// MSI Revert to Single Message
#define AHCI_HC_HC_MRSM         (1U<<AHCI_HC_HC_MRSM_BIT)

// Interrupt Enable
#define AHCI_HC_HC_IE           (1U<<AHCI_HC_HC_IE_BIT)

// HBA Reset
#define AHCI_HC_HC_HR           (1U<<AHCI_HC_HC_HR_BIT)

//
// hba_port_t::intr_status
#define AHCI_HP_IS_CPDS_BIT     31
#define AHCI_HP_IS_TFES_BIT     30
#define AHCI_HP_IS_HBFS_BIT     29
#define AHCI_HP_IS_HBDS_BIT     28
#define AHCI_HP_IS_IFS_BIT      27
#define AHCI_HP_IS_INFS_BIT     26
#define AHCI_HP_IS_OFS_BIT      24
#define AHCI_HP_IS_IPMS_BIT     23
#define AHCI_HP_IS_PRCS_BIT     22
#define AHCI_HP_IS_DMPS_BIT     7
#define AHCI_HP_IS_PCS_BIT      6
#define AHCI_HP_IS_DPS_BIT      5
#define AHCI_HP_IS_UFS_BIT      4
#define AHCI_HP_IS_SDBS_BIT     3
#define AHCI_HP_IS_DSS_BIT      2
#define AHCI_HP_IS_PSS_BIT      1
#define AHCI_HP_IS_DHRS_BIT     0

// Cold Port Detect Status
#define AHCI_HP_IS_CPDS         (1U<<AHCI_HP_IS_CPDS_BIT)

// Task File Error Status
#define AHCI_HP_IS_TFES         (1U<<AHCI_HP_IS_TFES_BIT)

// Host Bus Fatal Error Status
#define AHCI_HP_IS_HBFS         (1U<<AHCI_HP_IS_HBFS_BIT)

// Host Bus Data Error Status
#define AHCI_HP_IS_HBDS         (1U<<AHCI_HP_IS_HBDS_BIT)

// Interface Fatal Error Status
#define AHCI_HP_IS_IFS          (1U<<AHCI_HP_IS_IFS_BIT)

// Interface Non-fatal Error Status
#define AHCI_HP_IS_INFS         (1U<<AHCI_HP_IS_INFS_BIT)

// Overflow Status
#define AHCI_HP_IS_OFS          (1U<<AHCI_HP_IS_OFS_BIT)

// Incorrect Port Multiplier Status
#define AHCI_HP_IS_IPMS         (1U<<AHCI_HP_IS_IPMS_BIT)

// PhyRdy Change Status
#define AHCI_HP_IS_PRCS         (1U<<AHCI_HP_IS_PRCS_BIT)

// Device Mechanical Presence Status
#define AHCI_HP_IS_DMPS         (1U<<AHCI_HP_IS_DMPS_BIT)

// Port Connect Change Status
#define AHCI_HP_IS_PCS          (1U<<AHCI_HP_IS_PCS_BIT)

// Descriptor Processed Status
#define AHCI_HP_IS_DPS          (1U<<AHCI_HP_IS_DPS_BIT)

// Unknown FIS Interrupt
#define AHCI_HP_IS_UFS          (1U<<AHCI_HP_IS_UFS_BIT)

// Set Device Bits Interrupt
#define AHCI_HP_IS_SDBS         (1U<<AHCI_HP_IS_SDBS_BIT)

// DMA Setup FIS Interrupt
#define AHCI_HP_IS_DSS          (1U<<AHCI_HP_IS_DSS_BIT)

// PIO Setup FIS Interrupt
#define AHCI_HP_IS_PSS          (1U<<AHCI_HP_IS_PSS_BIT)

// Device to Host Register FIS Interrupt
#define AHCI_HP_IS_DHRS         (1U<<AHCI_HP_IS_DHRS_BIT)

//
// hba_port_t::intr_en
#define AHCI_HP_IE_CPDS_BIT     31
#define AHCI_HP_IE_TFES_BIT     30
#define AHCI_HP_IE_HBFS_BIT     29
#define AHCI_HP_IE_HBDS_BIT     28
#define AHCI_HP_IE_IFS_BIT      27
#define AHCI_HP_IE_INFS_BIT     26
#define AHCI_HP_IE_OFS_BIT      24
#define AHCI_HP_IE_IPMS_BIT     23
#define AHCI_HP_IE_PRCS_BIT     22
#define AHCI_HP_IE_DMPS_BIT     7
#define AHCI_HP_IE_PCS_BIT      6
#define AHCI_HP_IE_DPS_BIT      5
#define AHCI_HP_IE_UFS_BIT      4
#define AHCI_HP_IE_SDBS_BIT     3
#define AHCI_HP_IE_DSS_BIT      2
#define AHCI_HP_IE_PSS_BIT      1
#define AHCI_HP_IE_DHRS_BIT     0

// Cold Port Detect interrupt enable
#define AHCI_HP_IE_CPDS         (1U<<AHCI_HP_IE_CPDS_BIT)

// Task File Error interrupt enable
#define AHCI_HP_IE_TFES         (1U<<AHCI_HP_IE_TFES_BIT)

// Host Bus Fatal Error interrupt enable
#define AHCI_HP_IE_HBFS         (1U<<AHCI_HP_IE_HBFS_BIT)

// Host Bus Data Error interrupt enable
#define AHCI_HP_IE_HBDS         (1U<<AHCI_HP_IE_HBDS_BIT)

// Interface Fatal Error interrupt enable
#define AHCI_HP_IE_IFS          (1U<<AHCI_HP_IE_IFS_BIT)

// Interface Non-fatal Error interrupt enable
#define AHCI_HP_IE_INFS         (1U<<AHCI_HP_IE_INFS_BIT)

// Overflow interrupt enable
#define AHCI_HP_IE_OFS          (1U<<AHCI_HP_IE_OFS_BIT)

// Incorrect Port Multiplier interrupt enable
#define AHCI_HP_IE_IPMS         (1U<<AHCI_HP_IE_IPMS_BIT)

// PhyRdy Change interrupt enable
#define AHCI_HP_IE_PRCS         (1U<<AHCI_HP_IE_PRCS_BIT)

// Device Mechanical Presence interrupt enable
#define AHCI_HP_IE_DMPS         (1U<<AHCI_HP_IE_DMPS_BIT)

// Port Connect Change interrupt enable
#define AHCI_HP_IE_PCS          (1U<<AHCI_HP_IE_PCS_BIT)

// Descriptor Processed interrupt enable
#define AHCI_HP_IE_DPS          (1U<<AHCI_HP_IE_DPS_BIT)

// Unknown FIS interrupt enable
#define AHCI_HP_IE_UFS          (1U<<AHCI_HP_IE_UFS_BIT)

// Set Device Bits interrupt enable
#define AHCI_HP_IE_SDBS         (1U<<AHCI_HP_IE_SDBS_BIT)

// DMA Setup FIS interrupt enable
#define AHCI_HP_IE_DSS          (1U<<AHCI_HP_IE_DSS_BIT)

// PIO Setup FIS interrupt enable
#define AHCI_HP_IE_PSS          (1U<<AHCI_HP_IE_PSS_BIT)

// Device to Host Register FIS interrupt enable
#define AHCI_HP_IE_DHRS         (1U<<AHCI_HP_IE_DHRS_BIT)

//
// hba_port_t::cmd
#define AHCI_HP_CMD_ICC_BIT     28
#define AHCI_HP_CMD_ASP_BIT     27
#define AHCI_HP_CMD_ALPE_BIT    26
#define AHCI_HP_CMD_DLAE_BIT    25
#define AHCI_HP_CMD_ATAPI_BIT   24
#define AHCI_HP_CMD_APSTE_BIT   23
#define AHCI_HP_CMD_FBSCP_BIT   22
#define AHCI_HP_CMD_ESP_BIT     21
#define AHCI_HP_CMD_CPD_BIT     20
#define AHCI_HP_CMD_MPSP_BIT    19
#define AHCI_HP_CMD_HPCP_BIT    18
#define AHCI_HP_CMD_PMA_BIT     17
#define AHCI_HP_CMD_CPS_BIT     16
#define AHCI_HP_CMD_CR_BIT      15
#define AHCI_HP_CMD_FR_BIT      14
#define AHCI_HP_CMD_MPSS_BIT    13
#define AHCI_HP_CMD_CCS_BIT     8
#define AHCI_HP_CMD_FRE_BIT     4
#define AHCI_HP_CMD_CLO_BIT     3
#define AHCI_HP_CMD_POD_BIT     2
#define AHCI_HP_CMD_SUD_BIT     1
#define AHCI_HP_CMD_ST_BIT      0

// Interface Communication Control
#define AHCI_HP_CMD_ICC         (1U<<AHCI_HP_CMD_ICC_BIT)

// Aggressive Slumber / Partial
#define AHCI_HP_CMD_ASP         (1U<<AHCI_HP_CMD_ASP_BIT)

// Aggressive Link Power Manager Enable
#define AHCI_HP_CMD_ALPE        (1U<<AHCI_HP_CMD_ALPE_BIT)

// Drive LED on ATAPI Enablr
#define AHCI_HP_CMD_DLAE        (1U<<AHCI_HP_CMD_DLAE_BIT)

// Device is ATAPI
#define AHCI_HP_CMD_ATAPI       (1U<<AHCI_HP_CMD_ATAPI_BIT)

// Automatic Partial to Slumber Transitions Enabled
#define AHCI_HP_CMD_APSTE       (1U<<AHCI_HP_CMD_APSTE_BIT)

// FIS Based Switching Capable Port
#define AHCI_HP_CMD_FBSCP       (1U<<AHCI_HP_CMD_FBSCP_BIT)

// External SATA Port
#define AHCI_HP_CMD_ESP         (1U<<AHCI_HP_CMD_ESP_BIT)

// Cold Presence Detect
#define AHCI_HP_CMD_CPD         (1U<<AHCI_HP_CMD_CPD_BIT)

// Mechanical Presence Switch attached to Port
#define AHCI_HP_CMD_MPSP        (1U<<AHCI_HP_CMD_MPSP_BIT)

// Hot Plug Capable Port
#define AHCI_HP_CMD_HPCP        (1U<<AHCI_HP_CMD_HPCP_BIT)

// Port Multiplier Attached
#define AHCI_HP_CMD_PMA         (1U<<AHCI_HP_CMD_PMA_BIT)

// Cold Presence State
#define AHCI_HP_CMD_CPS         (1U<<AHCI_HP_CMD_CPS_BIT)

// Command List Running
#define AHCI_HP_CMD_CR          (1U<<AHCI_HP_CMD_CR_BIT)

// FIS Receive Running
#define AHCI_HP_CMD_FR          (1U<<AHCI_HP_CMD_FR_BIT)

// Mechanical Presense Switch State
#define AHCI_HP_CMD_MPSS        (1U<<AHCI_HP_CMD_MPSS_BIT)

// Current Command Slot
#define AHCI_HP_CMD_CCS         (1U<<AHCI_HP_CMD_CCS_BIT)

// FIS Receive Enable
#define AHCI_HP_CMD_FRE         (1U<<AHCI_HP_CMD_FRE_BIT)

// Command List Override
#define AHCI_HP_CMD_CLO         (1U<<AHCI_HP_CMD_CLO_BIT)

// Power On Device
#define AHCI_HP_CMD_POD         (1U<<AHCI_HP_CMD_POD_BIT)

// Spin Up Device
#define AHCI_HP_CMD_SUD         (1U<<AHCI_HP_CMD_SUD_BIT)

// Start
#define AHCI_HP_CMD_ST          (1U<<AHCI_HP_CMD_ST_BIT)

//
// hba_port_t::taskfile_data

#define AHCI_HP_TFD_ERR_BIT     8
#define AHCI_HP_TFD_ERR_BITS    8
#define AHCI_HP_TFD_ERR_MASK    ((1U << AHCI_HP_TFD_ERR_BITS)-1)
#define AHCI_HP_TFD_ERR         (AHCI_HP_TFD_ERR_MASK << AHCI_HP_TFD_ERR_BIT)
#define AHCI_HP_TFD_ERR_n(n)    ((n) << AHCI_HP_TFD_ERR_BIT)

#define AHCI_HP_TFD_SBSY_BIT    7
#define AHCI_HP_TFD_SCS64_BIT   4
#define AHCI_HP_TFD_SDRQ_BIT    3
#define AHCI_HP_TFD_SCS21_BIT   1
#define AHCI_HP_TFD_SERR_BIT    0

#define AHCI_HP_TFD_SBSY        (1U << AHCI_HP_TFD_SBSY_BIT)
#define AHCI_HP_TFD_SDRQ        (1U << AHCI_HP_TFD_SDRQ_BIT)
#define AHCI_HP_TFD_SERR        (1U << AHCI_HP_TFD_SERR_BIT)

//
// hba_port_t::sata_ctl

#define AHCI_HP_SC_IPM_BIT      8
#define AHCI_HP_SC_SPD_BIT      4
#define AHCI_HP_SC_DET_BIT      0

#define AHCI_HP_SC_IPM_BITS     4
#define AHCI_HP_SC_SPD_BITS     4
#define AHCI_HP_SC_DET_BITS     4

#define AHCI_HP_SC_IPM_MASK     ((1U<<AHCI_HP_SC_IPM_BITS)-1)
#define AHCI_HP_SC_SPD_MASK     ((1U<<AHCI_HP_SC_SPD_BITS)-1)
#define AHCI_HP_SC_DET_MASK     ((1U<<AHCI_HP_SC_DET_BITS)-1)

#define AHCI_HP_SC_IPM          (AHCI_HP_SC_IPM_MASK<<AHCI_HP_SC_IPM_BIT)
#define AHCI_HP_SC_SPD          (AHCI_HP_SC_SPD_MASK<<AHCI_HP_SC_SPD_BIT)
#define AHCI_HP_SC_DET          (AHCI_HP_SC_DET_MASK<<AHCI_HP_SC_DET_BIT)

#define AHCI_HP_SC_IPM_n(n)     ((n)<<AHCI_HP_SC_IPM_BIT)
#define AHCI_HP_SC_SPD_n(n)     ((n)<<AHCI_HP_SC_SPD_BIT)
#define AHCI_HP_SC_DET_n(n)     ((n)<<AHCI_HP_SC_DET_BIT)

//
// AHCI_HP_SC_IPM_n

// Allow all sleep states
#define AHCI_HP_SC_IPM_ALL      0

// No Partial state
#define AHCI_HP_SC_IPM_NO_P     1

// No Slumber state
#define AHCI_HP_SC_IPM_NO_S     2

// No Partial or Slumber state
#define AHCI_HP_SC_IPM_NO_PS    3

// No DevSleep state
#define AHCI_HP_SC_IPM_NO_D     4

// No Partial or DevSleep state
#define AHCI_HP_SC_IPM_NO_PD    5

// No Slumber or DevSleep state
#define AHCI_HP_SC_IPM_NO_SD    6

// No Partial, Slumber, or DevSleep state
#define AHCI_HP_SC_IPM_NO_PSD   7

//
// AHCI_HP_SC_SPD_n

// Allow all speeds
#define AHCI_HP_SC_SPD_ALL      0

// Limit to Generation 1 speed
#define AHCI_HP_SC_SPD_GEN1     1

// Limit to Generation 2 speed
#define AHCI_HP_SC_SPD_GEN2     2

// Limit to Generation 3 speed
#define AHCI_HP_SC_SPD_GEN3     3

//
// AHCI_HP_SC_DET_n

// Operate normally
#define AHCI_HP_SC_DET_NONE     0

// Perform interface initialization
#define AHCI_HP_SC_DET_INIT     1

// Disable the ATA interface
#define AHCI_HP_SC_DET_OFFLINE  4

// Identify fields
#define ATA_IDENT_NCQ_DEP_IDX   75
#define ATA_IDENT_CAPS_IDX      76

#define ATA_IDENT_NCQ_DEP_BIT   0
#define ATA_IDENT_NCQ_DEP_BITS  5
#define ATA_IDENT_NCQ_DEP_MASK  (1<<ATA_IDENT_NCQ_DEP_BITS)
#define ATA_IDENT_NCQ_DEP_n(n)  (((n) & ATA_IDENT_NCQ_DEP_MASK) + 1)

// 4.2.1
struct hba_fis_t {
    // offset = 0x00
    ahci_dma_setup_t dsfis;
    uint8_t pad0[4];

    // offset = 0x20
    ahci_pio_setup_t psfis;
    uint8_t pad1[12];

    // offset = 0x40
    // Register – Device to Host FIS
    ahci_fis_d2h_t rfis;
    uint8_t pad2[4];

    // offset = 0x58
    // 0x58 (not documented)
    uint8_t sdbfis[8];

    // offset = 0x60
    // Unknown fis buffer
    uint8_t ufis[64];

    uint8_t rsv[0x100-0xA0];
};

C_ASSERT(offsetof(hba_fis_t, dsfis) == 0x00);
C_ASSERT(offsetof(hba_fis_t, psfis) == 0x20);
C_ASSERT(sizeof(ahci_pio_setup_t) == 0x14);
C_ASSERT(offsetof(hba_fis_t, rfis) == 0x40);
C_ASSERT(sizeof(ahci_fis_d2h_t) == 0x14);
C_ASSERT(offsetof(hba_fis_t, sdbfis) == 0x58);
C_ASSERT(offsetof(hba_fis_t, ufis) == 0x60);
C_ASSERT(offsetof(hba_fis_t, rsv) == 0xA0);
C_ASSERT(sizeof(hba_fis_t) == 0x100);

// 4.2.2
struct hba_cmd_hdr_t {
    // Header
    uint16_t hdr;

    // Physical Region Descriptor Table Length (entries)
    uint16_t prdtl;

    // PRD Byte Count
    uint32_t prdbc;

    // Command Table Base Address (128 byte aligned)
    uint64_t ctba;

    uint32_t rsv[4];
};

C_ASSERT(sizeof(hba_cmd_hdr_t) == 32);

//
// hba_cmd_hdr_t::hdr

// Port multiplier port
#define AHCI_CH_PMP_BIT     12

// Clear busy on R_OK
#define AHCI_CH_CB_BIT      10

// Built In Self Test
#define AHCI_CH_BIST_BIT    9

// Reset
#define AHCI_CH_RST_BIT     8

// Prefetchable
#define AHCI_CH_PF_BIT      7

// Write
#define AHCI_CH_WR_BIT      6

// ATAPI
#define AHCI_CH_ATAPI_BIT   5

// FIS length
#define AHCI_CH_LEN_BIT     0

#define AHCI_CH_PMP_BITS    4
#define AHCI_CH_PMP_MASK    ((1U<<AHCI_CH_PMP_BIT)-1)
#define AHCI_CH_PMP_n(n)    ((n)<<AHCI_CH_PMP_BIT)

#define AHCI_CH_LEN_BITS    5
#define AHCI_CH_LEN_MASK    ((1U<<AHCI_CH_LEN_BITS)-1)
#define AHCI_CH_LEN_n(n)    ((n)<<AHCI_CH_LEN_BIT)

#define AHCI_CH_PMP         (AHCI_CH_PMP_MASK<<AHCI_CH_PMP_BIT)
#define AHCI_CH_CB          (1U<<AHCI_CH_CB_BIT)
#define AHCI_CH_BIST        (1U<<AHCI_CH_BIST_BIT)
#define AHCI_CH_RST         (1U<<AHCI_CH_RST_BIT)
#define AHCI_CH_PF          (1U<<AHCI_CH_PF_BIT)
#define AHCI_CH_WR          (1U<<AHCI_CH_WR_BIT)
#define AHCI_CH_ATAPI       (1U<<AHCI_CH_ATAPI_BIT)
#define AHCI_CH_LEN         (AHCI_CH_LEN_MASK<<AHCI_CH_LEN_BIT)

//
// hba_cmd_hdr_t::prdbc

// Data Byte Count
#define AHCI_CH_DBC_n(n)    (((n)<<1) | 1)

// 4.2.3.3 Physical Region Descriptor Table Entry
struct hba_prdt_ent_t {
    // Data Base Address
    uint64_t dba;
    uint32_t rsv;
    // Data Byte Count
    uint32_t dbc_intr;
};

// Makes command table entry 1KB
#define AHCI_CMD_TBL_ENT_MAX_PRD 56

union hba_cmd_cfis_t {
    ahci_fis_d2h_t d2h;
    ahci_fis_h2d_t h2d;
    ahci_fis_ncq_t ncq;
    char filler[64];
};

// 4.2.3 1KB command
struct hba_cmd_tbl_ent_t {
    hba_cmd_cfis_t cfis;
    atapi_fis_t atapi_fis;
    char filler[0x80-0x50];

    // At offset 0x80
    hba_prdt_ent_t prdts[AHCI_CMD_TBL_ENT_MAX_PRD];
};

C_ASSERT(sizeof(hba_cmd_tbl_ent_t) == 1024);

typedef void (*async_callback_fn_t)(int error, uintptr_t arg);

struct async_callback_t {
    async_callback_fn_t callback;
    uintptr_t callback_arg;
    int error;
};

enum struct slot_op_t {
    read,
    write,

    // All non-NCQ operations must go after this value
    non_ncq,

    identify,
    flush
};

struct slot_request_t {
    void *data;
    int64_t count;
    uint64_t lba;
    slot_op_t op;
    bool fua;
    async_callback_t callback;
};

struct hba_port_info_t {
    hba_fis_t *fis;
    hba_cmd_hdr_t *cmd_hdr;
    hba_cmd_tbl_ent_t *cmd_tbl;
    uint32_t is_atapi;
    uint32_t cmd_issued;
    uint32_t slot_mask;

    slot_request_t slot_requests[32];

    // Wake every time a slot is released
    condition_var_t slotalloc_avail;

    // Wake when all slots are released
    condition_var_t idle_cond;

    // Wake after a non-NCQ command finishes (when NCQ capable)
    condition_var_t non_ncq_done_cond;

    // Keep track of slot order
    mutex_t lock;
    uint8_t issue_queue[32];
    uint8_t issue_head;
    uint8_t issue_tail;

    // Set to true to block slot acquires to idle the controller for a
    // non-NCQ command (when NCQ is enabled). non_ncq_done_cond is signalled
    // when the command is finished
    bool non_ncq_pending;

    bool use_ncq;
    bool use_fua;
    bool use_48bit;
    uint8_t queue_depth;
    uint8_t log2_sector_size;
};

#define AHCI_PE_INTR_BIT    31
#define AHCI_PE_DBC_BIT     1
#define AHCI_PE_DBC_n(n)    ((n)-1)

class ahci_if_factory_t : public storage_if_factory_t {
public:
    ahci_if_factory_t() : storage_if_factory_t("ahci") {}
private:
    virtual if_list_t detect(void) final;
};

ahci_if_factory_t ahci_factory;

// AHCI interface instance
class ahci_if_t : public storage_if_base_t {
public:
    void init(pci_dev_iterator_t const &pci_iter);

    unsigned io(unsigned port_num, slot_request_t &request);

    int port_flush(unsigned port_num, async_callback_fn_t callback, uintptr_t callback_arg);

    unsigned get_sector_size(unsigned port);
    void configure_ncq(unsigned port_num, bool enable, uint8_t queue_depth);
    void configure_48bit(unsigned port_num, bool enable);
    void configure_fua(unsigned port_num, bool enable);

    int8_t slot_wait(hba_port_info_t &pi);

private:
    STORAGE_IF_IMPL

    unsigned io_locked(unsigned port_num, slot_request_t &request);

    bool supports_64bit();
    void slot_release(unsigned port_num, int slot);
    void handle_port_irqs(unsigned port_num);
    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    void port_stop(unsigned port_num);
    void port_start(unsigned port_num);
    void port_stop_all();
    void port_start_all();
    void port_reset(unsigned port_num);
    void rebase();

    void bios_handoff();

    int8_t slot_acquire(hba_port_info_t& pi);

    void cmd_issue(unsigned port_num, unsigned slot,
                   hba_cmd_cfis_t const *cfis, atapi_fis_t const *atapi_fis,
                   size_t fis_size, hba_prdt_ent_t const *prdts,
                   size_t ranges_count);

    pci_config_hdr_t config;

    // Linear addresses
    hba_host_ctl_t volatile *mmio_base;

    void *buffers;

    hba_port_info_t port_info[32];

    pci_irq_range_t irq_range;
    uint32_t ports_impl_mask;
    uint8_t num_cmd_slots;
    bool use_msi;
    bool support_ncq;
    bool use_64;
};

// Drive
class ahci_dev_t : public storage_dev_base_t {
public:
    void init(ahci_if_t *parent, unsigned dev_port, bool dev_is_atapi);

private:
    STORAGE_DEV_IMPL

    int io(void *data, int64_t count,
           uint64_t lba, bool fua, slot_op_t op);

    ahci_if_t *iface;
    unsigned port;
    bool is_atapi;
};

#define AHCI_MAX_DEVICES    16
static ahci_if_t ahci_devices[AHCI_MAX_DEVICES];
static unsigned ahci_count;

#define AHCI_MAX_DRIVES    64
static ahci_dev_t ahci_drives[AHCI_MAX_DRIVES];
static unsigned ahci_drive_count;

bool ahci_if_t::supports_64bit()
{
    return (mmio_base->cap & AHCI_HC_CAP_S64A) != 0;
}

// Must be holding port lock
void ahci_if_t::slot_release(unsigned port_num, int slot)
{
    assert(slot >= 0);

    hba_port_info_t *pi = port_info + port_num;

    // Make sure it was really acquired
    assert(pi->cmd_issued & (1U<<slot));

    if (!pi->use_ncq) {
        // Make sure this was really the oldest command
        assert(pi->issue_queue[pi->issue_tail] == slot);
        // Advance queue tail
        pi->issue_tail = (pi->issue_tail + 1) & 31;
    }

    // Mark slot as not in use
    pi->cmd_issued &= ~(1U<<slot);

    // Wake a thread that may be waiting for a slot
    condvar_wake_one(&pi->slotalloc_avail);

    // Wake non-ncq thread if non-ncq is pending when every slot is free
    if (pi->non_ncq_pending && ((pi->cmd_issued & pi->slot_mask) == 0))
        condvar_wake_one(&pi->idle_cond);
}

// Acquire slot that is not in use
// Returns -1 if all slots are in use
// Must be holding port lock
int8_t ahci_if_t::slot_acquire(hba_port_info_t& pi)
{
    // Wait for non-NCQ command to finish
    while (unlikely(pi.use_ncq && pi.non_ncq_pending))
        condvar_wait(&pi.non_ncq_done_cond, &pi.lock);

    // Build bitmask of slots in use
    uint32_t busy_mask = pi.cmd_issued;

    // If every slot is busy
    if (unlikely(busy_mask == 0xFFFFFFFF))
        return -1;

    // Find first zero
    uint8_t slot = bit_lsb_set(~busy_mask);

    pi.cmd_issued |= (1U << slot);

    if (!pi.use_ncq) {
        // Write slot number to issue queue
        pi.issue_queue[pi.issue_head] = slot;
        pi.issue_head = (pi.issue_head + 1) & 31;
    }

    return slot;
}

void ahci_if_t::handle_port_irqs(unsigned port_num)
{
    hba_host_ctl_t volatile *hba = mmio_base;
    hba_port_t volatile *port = hba->ports + port_num;
    hba_port_info_t *pi = port_info + port_num;

    async_callback_t pending_callbacks[32];
    unsigned callback_count = 0;

    mutex_lock_noyield(&pi->lock);

    uint32_t port_intr_status = port->intr_status;

    // Read command slot interrupt status
    uint8_t slot;

    if (pi->use_ncq) {
        // Handle all successes first
        for (uint32_t done_slots = pi->slot_mask & pi->cmd_issued &
             (pi->cmd_issued ^ port->sata_act);
             done_slots; done_slots &= ~(1U << slot)) {
            slot = bit_lsb_set(done_slots);

            int error = (port->sata_err != 0);
            assert(error == 0);

            slot_request_t &request = pi->slot_requests[slot];

            request.callback.error = error;

            // Invoke completion callback
            assert(request.callback.callback);
            assert(callback_count < countof(pending_callbacks));
            pending_callbacks[callback_count++] = request.callback;
            request.callback.callback = 0;
            request.callback.callback_arg = 0;
            request.callback.error = 0;

            slot_release(port_num, slot);
        }

        if (unlikely(port_intr_status & AHCI_HP_IS_TFES)) {
            /// Handle failure by
            ///  - stopping DMA engine,
            ///  - disabling ncq
            ///  - resetting port
            ///  - starting DMA engine
            ///  - rebuilding issue queue (for non-NCQ completion)
            ///  - reissuing all outstanding commands
            /// This is to make command execution sequential, to get precise
            /// error information
            port_stop(port_num);
            port_reset(port_num);
            pi->use_ncq = false;
            port_start(port_num);

            // Free up all ports and reinitialize issue_queue
            uint32_t pending_mask = pi->cmd_issued;
            pi->cmd_issued = 0;
            pi->issue_head = 0;
            pi->issue_tail = 0;

            // Take a snapshot of all pending commands
            slot_request_t pending[32];
            C_ASSERT(sizeof(pending) == sizeof(pi->slot_requests));
            memcpy(pending, pi->slot_requests, sizeof(pending));

            // Reissue each command in order
            for (uint8_t reissue = 0; reissue < num_cmd_slots; ++reissue) {
                if (!(pending_mask & (1U << reissue)))
                    continue;

                slot_request_t &request = pending[reissue];

                io_locked(port_num, request);
            }
        }
    } else {
        slot = pi->issue_queue[pi->issue_tail];

        slot_request_t &request = pi->slot_requests[slot];

        int error = 0;

        if (unlikely(port_intr_status & AHCI_HP_IS_TFES) &&
                (port->taskfile_data & AHCI_HP_TFD_SERR)) {
            // Taskfile error
            error = (port->taskfile_data >> AHCI_HP_TFD_ERR_BIT) &
                    AHCI_HP_TFD_ERR_MASK;

            AHCI_TRACE("Error: port=%u, err=%x\n", port_num, error);
        }

        if (error != 0) {
            AHCI_TRACE("Error %d on interface=%zu port=%zu\n",
                       error, this - ahci_devices,
                       pi - port_info);
        }

        request.callback.error = error;

        // Invoke completion callback
        assert(request.callback.callback);
        assert(callback_count < countof(pending_callbacks));
        pending_callbacks[callback_count++] = request.callback;
        request.callback.callback = 0;
        request.callback.callback_arg = 0;
        request.callback.error = 0;

        slot_release(port_num, slot);

        if (pi->use_ncq && int(request.op) > int(slot_op_t::non_ncq)) {
            pi->non_ncq_pending = false;
            condvar_wake_all(&pi->non_ncq_done_cond);
        }
    }

    // Acknowledge slot interrupt
    port->intr_status |= port_intr_status;

    mutex_unlock(&pi->lock);

    // Make all callbacks outside lock
    for (unsigned i = 0; i < callback_count; ++i) {
        async_callback_t *callback = pending_callbacks + i;
        if (callback->callback) {
            callback->callback(callback->error,
                               callback->callback_arg);
        }
    }
}

isr_context_t *ahci_if_t::irq_handler(int irq, isr_context_t *ctx)
{
    for (unsigned i = 0; i < ahci_count; ++i) {
        ahci_if_t *dev = ahci_devices + i;

        int irq_offset = irq - dev->irq_range.base;

        if (unlikely(irq_offset < 0 || irq_offset > dev->irq_range.count))
            continue;

        // Call callback on every port that has an interrupt pending
        unsigned port;
        for (uint32_t intr_status = dev->mmio_base->intr_status;
             intr_status != 0; intr_status &= ~(1U << port)) {
            // Look through each port
            port = bit_lsb_set(intr_status);

            dev->handle_port_irqs(port);

            // Acknowledge the interrupt on the port
            dev->mmio_base->intr_status = (1U << port);
        }
    }

    return ctx;
}

void ahci_if_t::port_stop(unsigned port_num)
{
    hba_port_t volatile *port = mmio_base->ports + port_num;

    // Clear start bit
    // Clear FIS receive enable bit
    port->cmd &= ~(AHCI_HP_CMD_ST | AHCI_HP_CMD_FRE);

    atomic_barrier();

    // Wait until there is not a command running,
    // and there is not a FIS receive running
    while (port->cmd & (AHCI_HP_CMD_CR | AHCI_HP_CMD_FR))
        pause();
}

void ahci_if_t::port_start(unsigned port_num)
{
    hba_port_t volatile *port = mmio_base->ports + port_num;

    // Wait until there is not a command running, and
    // there is not a FIS receive running
    while (port->cmd & (AHCI_HP_CMD_CR | AHCI_HP_CMD_FR))
        pause();

    // Set start and FIS receive enable bit
    port->cmd |= AHCI_HP_CMD_ST | AHCI_HP_CMD_FRE;
}

// Stop each implemented port
void ahci_if_t::port_stop_all()
{
    unsigned port;
    for (uint32_t impl = ports_impl_mask;
         impl != 0; impl &= ~(1U<<port)) {
        port = bit_lsb_set_32(impl);
        port_stop(port);
    }
}

// Start each implemented port
void ahci_if_t::port_start_all()
{
    unsigned port;
    for (uint32_t impl = ports_impl_mask;
         impl != 0; impl &= ~(1U<<port)) {
        port = bit_lsb_set_32(impl);
        port_start(port);
    }
}

// Must be holding port lock
void ahci_if_t::cmd_issue(unsigned port_num, unsigned slot,
        hba_cmd_cfis_t const *cfis, atapi_fis_t const *atapi_fis,
        size_t fis_size, hba_prdt_ent_t const *prdts, size_t ranges_count)
{
    hba_port_info_t *pi = port_info + port_num;
    hba_port_t volatile *port = mmio_base->ports + port_num;

    hba_cmd_hdr_t *cmd_hdr = pi->cmd_hdr + slot;
    hba_cmd_tbl_ent_t *cmd_tbl_ent = pi->cmd_tbl + slot;

    slot_request_t &request = pi->slot_requests[slot];

    if (likely(prdts != nullptr))
        memcpy(cmd_tbl_ent->prdts, prdts, sizeof(cmd_tbl_ent->prdts));
    else
        memset(cmd_tbl_ent->prdts, 0, sizeof(cmd_tbl_ent->prdts));

    memcpy(&cmd_tbl_ent->cfis, cfis, sizeof(*cfis));

    if (atapi_fis)
        cmd_tbl_ent->atapi_fis = *atapi_fis;

    cmd_hdr->hdr = AHCI_CH_LEN_n(fis_size >> 2) |
            (request.op == slot_op_t::write ? AHCI_CH_WR : 0) |
            (atapi_fis ? AHCI_CH_ATAPI : 0);
    cmd_hdr->prdbc = 0;
    cmd_hdr->prdtl = ranges_count;

    request.callback.error = 0;

    atomic_barrier();

    if (pi->use_ncq)
        port->sata_act = (1U<<slot);

    atomic_barrier();
    port->cmd_issue = (1U<<slot);
}

unsigned ahci_if_t::get_sector_size(unsigned port)
{
    return 1U << port_info[port].log2_sector_size;
}

void ahci_if_t::configure_48bit(unsigned port_num, bool enable)
{
    port_info[port_num].use_48bit = enable;
}

void ahci_if_t::configure_fua(unsigned port_num, bool enable)
{
    port_info[port_num].use_fua = enable;
}

void ahci_if_t::configure_ncq(unsigned port_num, bool enable,
                              uint8_t queue_depth)
{
    port_info[port_num].use_ncq = enable && support_ncq;
    port_info[port_num].queue_depth = queue_depth;

    // Use queue depth to mark all unsupported tags permanently busy
    if (queue_depth < 32 && enable) {
        // Mark all unsupported slots permanently busy
        port_info[port_num].cmd_issued |= ~0 << queue_depth;

        // Setup mask of usable slots
        port_info[port_num].slot_mask = ~(~0 << queue_depth);
    } else {
        port_info[port_num].slot_mask = ~0U;
    }
}

void ahci_if_t::init(pci_dev_iterator_t const &pci_iter)
{
    config = pci_iter.config;

    mmio_base = (hba_host_ctl_t*)
            mmap((void*)(uintptr_t)pci_iter.config.base_addr[5],
            0x1100, PROT_READ | PROT_WRITE,
            MAP_PHYSICAL, -1, 0);

    // Set AHCI enable in GHC
    mmio_base->host_ctl |= AHCI_HC_HC_AE;

    AHCI_TRACE("Performing BIOS handoff\n");
    bios_handoff();

    // Cache implemented port bitmask
    ports_impl_mask = mmio_base->ports_impl_mask;

    // Cache number of command slots per port
    num_cmd_slots = 1 + ((mmio_base->cap >>
                          AHCI_HC_CAP_NCS_BIT) &
                          AHCI_HC_CAP_NCS_MASK);

    use_64 = (mmio_base->cap & AHCI_HC_CAP_S64A) != 0;
    support_ncq = (mmio_base->cap & AHCI_HC_CAP_SNCQ) != 0;

    rebase();

    // Assume legacy IRQ pin usage until MSI succeeds
    irq_range.base = pci_iter.config.irq_line;
    irq_range.count = 1;

    // Try to use MSI IRQ
    use_msi = pci_set_msi_irq(
                pci_iter.bus, pci_iter.slot, pci_iter.func,
                &irq_range, 1, 0, 0,
                &ahci_if_t::irq_handler);

    if (!use_msi) {
        // Fall back to pin based IRQ
        irq_hook(irq_range.base, &ahci_if_t::irq_handler);
        irq_setmask(irq_range.base, 1);
    }
}

unsigned ahci_if_t::io(unsigned port_num, slot_request_t &request)
{
    unsigned expect_count;

    mutex_lock(&port_info[port_num].lock);

    expect_count = io_locked(port_num, request);

    mutex_unlock(&port_info[port_num].lock);

    return expect_count;
}

int ahci_if_t::port_flush(unsigned port_num, async_callback_fn_t callback,
                          uintptr_t callback_arg)
{
    hba_port_info_t &pi = port_info[port_num];

    mutex_lock(&pi.lock);

    bool save_ncq = false;

    if (pi.use_ncq) {
        if (!pi.non_ncq_pending)
            pi.non_ncq_pending = true;

        // Wait for all slots to become available
        while (pi.cmd_issued & pi.slot_mask)
            condvar_wait(&pi.idle_cond, &pi.lock);

        // Disable NCQ for duration of flush
        save_ncq = pi.use_ncq;
        pi.use_ncq = false;

        // Add item to in-order completion queue
        pi.issue_queue[pi.issue_head] = 0;
        pi.issue_head = (pi.issue_head + 1) & 31;
    }

    slot_request_t request = pi.slot_requests[0];

    memset(&request, 0, sizeof(request));
    request.op = slot_op_t::flush;
    request.callback.callback = callback;
    request.callback.callback_arg = callback_arg;

    io(port_num, request);

    if (save_ncq)
        pi.use_ncq = save_ncq;

    mutex_unlock(&pi.lock);

    return 0;
}

// Acquire a slot, waiting if necessary
int8_t ahci_if_t::slot_wait(hba_port_info_t &pi)
{
    int8_t slot;
    for (;;) {
        slot = slot_acquire(pi);
        if (slot >= 0)
            break;

        condvar_wait(&pi.slotalloc_avail, &pi.lock);
    }

    return slot;
}

// Expects interrupts disabled
// Returns the number of async completions to expect
unsigned ahci_if_t::io_locked(unsigned port_num, slot_request_t &request)
{
    mmphysrange_t ranges[AHCI_CMD_TBL_ENT_MAX_PRD];
    size_t ranges_count;

    hba_prdt_ent_t prdts[AHCI_CMD_TBL_ENT_MAX_PRD];

    hba_port_info_t &pi = port_info[port_num];

    // Make sure phy state is established
    if (unlikely((mmio_base->ports[port_num].sata_status & AHCI_SS_DET) !=
                 AHCI_SS_DET_n(AHCI_SS_DET_ONLINE))) {
        // Not established
        request.callback.callback(-int(errno_t::EIO),
                                  request.callback.callback_arg);
        return 0;
    }

    void *data = request.data;
    uint32_t lba = request.lba;
    uint32_t count = request.count;

    unsigned chunks;
    for (chunks = 0; count > 0; ++chunks) {
        if (likely(request.data != nullptr)) {
            ranges_count = mphysranges(ranges, countof(ranges),
                                       request.data,
                                       request.count << pi.log2_sector_size,
                                       4<<20);
        } else {
            ranges_count = 0;
        }

        size_t transferred = 0;

        memset(prdts, 0, sizeof(prdts));
        for (size_t i = 0; i < ranges_count; ++i) {
            prdts[i].dba = ranges[i].physaddr;
            prdts[i].dbc_intr = AHCI_PE_DBC_n(ranges[i].size);

            transferred += ranges[i].size;
        }

        size_t transferred_blocks = transferred >> pi.log2_sector_size;

        // Wait for a slot
        uint8_t slot = slot_wait(pi);

        hba_cmd_cfis_t cfis;
        size_t fis_size;

        memset(&cfis, 0, sizeof(cfis));

        atapi_fis_t atapifis;

        if (unlikely(request.op == slot_op_t::identify)) {
            fis_size = sizeof(cfis.h2d);

            cfis.h2d.fis_type = FIS_TYPE_REG_H2D;
            cfis.h2d.ctl = AHCI_FIS_CTL_CMD;

            cfis.h2d.command = !pi.is_atapi
                    ? ata_cmd_t::IDENTIFY
                    : ata_cmd_t::IDENTIFY_PACKET;
            cfis.h2d.set_lba(pi.is_atapi ? 512 << 8 : 0);
            cfis.h2d.set_count(0);
            cfis.h2d.feature_lo = 0;

            cfis.d2h.device = 0;
        } else if (unlikely(request.op == slot_op_t::flush)) {
            fis_size = sizeof(cfis.h2d);

            cfis.h2d.fis_type = FIS_TYPE_REG_H2D;
            cfis.h2d.ctl = AHCI_FIS_CTL_CMD;

            cfis.h2d.command = pi.use_48bit
                    ? ata_cmd_t::CACHE_FLUSH_EXT
                    : ata_cmd_t::CACHE_FLUSH;
        } else if (pi.use_ncq) {
            fis_size = sizeof(cfis.ncq);

            cfis.ncq.fis_type = FIS_TYPE_REG_H2D;
            cfis.ncq.ctl = AHCI_FIS_CTL_CMD;

            cfis.ncq.command = request.op == slot_op_t::read
                    ? ata_cmd_t::READ_DMA_NCQ
                    : ata_cmd_t::WRITE_DMA_NCQ;
            cfis.ncq.set_lba(request.lba);
            cfis.ncq.set_count(transferred_blocks);
            cfis.ncq.tag = AHCI_FIS_TAG_TAG_n(slot);
            cfis.ncq.fua = AHCI_FIS_FUA_LBA | (request.fua
                                               ? AHCI_FIS_FUA_FUA : 0);
            cfis.ncq.prio = 0;
            cfis.ncq.aux = 0;
        } else if (pi.is_atapi) {
            fis_size = sizeof(cfis.h2d);

            cfis.h2d.fis_type = FIS_TYPE_REG_H2D;
            cfis.h2d.ctl = AHCI_FIS_CTL_CMD;
            cfis.h2d.command = ata_cmd_t::PACKET;

            atapifis.set(ATAPI_CMD_READ, request.lba, transferred_blocks, 1);
            // DMA and DMADIR
            cfis.h2d.feature_lo = 1 | ((request.op == slot_op_t::read) << 2);
            cfis.h2d.set_count(0);
            cfis.h2d.set_lba(2048 << 16);
            cfis.d2h.device = 0;
        } else {
            fis_size = sizeof(cfis.h2d);

            cfis.h2d.fis_type = FIS_TYPE_REG_H2D;
            cfis.h2d.ctl = AHCI_FIS_CTL_CMD;

            cfis.h2d.command = request.op == slot_op_t::read
                    ? ata_cmd_t::READ_DMA_EXT
                    : ata_cmd_t::WRITE_DMA_EXT;
            assert(request.lba < (1UL << 48));
            cfis.h2d.set_lba(request.lba);
            cfis.h2d.set_count(transferred_blocks);
            cfis.h2d.feature_lo = 1;

            // LBA
            cfis.d2h.device = request.fua ? AHCI_FIS_FUA_LBA : 0;
        }

        atomic_barrier();

        pi.slot_requests[slot] = request;

        cmd_issue(port_num, slot, &cfis,
                  (request.op == slot_op_t::read &&
                   pi.is_atapi) ? &atapifis : 0,
                  fis_size, prdts, ranges_count);

        data = (char*)data + transferred;
        lba += transferred_blocks;
        count -= transferred_blocks;
    }

    return chunks;
}

// The command engine must be stopped before calling port_reset
void ahci_if_t::port_reset(unsigned port_num)
{
    hba_port_t volatile *port = mmio_base->ports + port_num;

    // Enable FIS receive
    port->cmd |= AHCI_HP_CMD_FRE;

    // Put port into INIT state
    port->sata_ctl = (port->sata_ctl & ~AHCI_HP_SC_DET) |
            AHCI_HP_SC_DET_n(AHCI_HP_SC_DET_INIT);

    // Wait 3x the documented minimum
    // FIXME: this isn't completely safe from IRQ handler
    usleep(3000);

    // Put port into normal operation
    port->sata_ctl = (port->sata_ctl & ~AHCI_HP_SC_DET) |
            AHCI_HP_SC_DET_n(0);

    // Clear FIS receive bit
    port->cmd &= ~AHCI_HP_CMD_FRE;

    // Acknowledge all interrupts
    port->intr_status = port->intr_status;
}

void ahci_if_t::rebase()
{
    AHCI_TRACE("Stopping all ports\n");
    // Stop all ports
    port_stop_all();

    bool support_64bit = supports_64bit();

    // QEMU bug workaround
    if (!support_64bit) {
        AHCI_TRACE("AHCI interface reports no 64-bit support!\n");
        support_64bit = true;
    }

    int addr_type = support_64bit
            ? MAP_POPULATE
            : MAP_POPULATE | MAP_32BIT;

    AHCI_TRACE("64 bit support: %d\n", support_64bit);

    // Loop through the implemented ports

    // Initial "slot busy" mask marks unimplemented slots
    // as permanently busy
    uint32_t init_busy_mask = (num_cmd_slots == 32)
            ? 0
            : ((~uint32_t(0))<<num_cmd_slots);

    unsigned port_num;
    for (uint32_t ports_impl = ports_impl_mask; ports_impl;
         ports_impl &= ~(1U << port_num)) {
        port_num = bit_lsb_set(ports_impl);

        AHCI_TRACE("Initializing AHCI device port %d\n", port_num);

        hba_port_t volatile *port = mmio_base->ports + port_num;
        hba_port_info_t *pi = port_info + port_num;

        mutex_init(&pi->lock);
        condvar_init(&pi->slotalloc_avail);
        condvar_init(&pi->idle_cond);
        condvar_init(&pi->non_ncq_done_cond);

        AHCI_TRACE("Performing detection, port %d\n", port_num);
        port_reset(port_num);

        pi->use_ncq = false;

        if (port->sig == ahci_sig_t::SATA_SIG_ATAPI) {
            pi->is_atapi = 1;
            pi->log2_sector_size = 11;
            port->cmd |= AHCI_HP_CMD_ATAPI;
        } else if (port->sig == ahci_sig_t::SATA_SIG_ATA) {
            pi->is_atapi = 0;
            pi->log2_sector_size = 9;
        } else {
            continue;
        }

        size_t port_buffer_size = 0;

        // 256 byte FIS buffer
        port_buffer_size += sizeof(hba_fis_t);
        // One cmd hdr per slot
        port_buffer_size += sizeof(hba_cmd_hdr_t) * 32;
        // One cmd tbl per slot
        port_buffer_size += sizeof(hba_cmd_tbl_ent_t) * 32;

        buffers = mmap(0, port_buffer_size,
                             PROT_READ | PROT_WRITE,
                             addr_type, -1, 0);
        memset(buffers, 0, port_buffer_size);

        hba_cmd_hdr_t *cmd_hdr = (hba_cmd_hdr_t *)buffers;
        hba_cmd_tbl_ent_t *cmd_tbl = (hba_cmd_tbl_ent_t*)(cmd_hdr + 32);
        hba_fis_t *fis = (hba_fis_t*)(cmd_tbl + 32);

        // Store linear addresses for writing to buffers
        pi->fis = fis;
        pi->cmd_hdr = cmd_hdr;
        pi->cmd_tbl = cmd_tbl;

        assert(port->sata_act == 0);

        // Initialize with all unimplemented slots busy
        pi->cmd_issued = init_busy_mask;
        pi->slot_mask = ~init_busy_mask;

        AHCI_TRACE("Setting cmd/FIS buffer addresses\n");

        atomic_barrier();
        port->cmd_list_base = mphysaddr(cmd_hdr);
        port->fis_base = mphysaddr(fis);
        atomic_barrier();

        // Set command table base addresses (physical)
        for (uint8_t slot = 0; slot < num_cmd_slots; ++slot)
            cmd_hdr[slot].ctba = mphysaddr(cmd_tbl + slot);

        // Clear SATA errors
        port->sata_err = port->sata_err;

        // Acknowledging interrupts
        port->intr_status = port->intr_status;

        AHCI_TRACE("Unmasking interrupts\n");

        port->intr_en = AHCI_HP_IE_TFES |   // taskfile error
                AHCI_HP_IE_HBFS |           // host bus fatal error
                AHCI_HP_IE_HBDS |           // host bus data error
                AHCI_HP_IE_IFS |            // interface fatal error
                AHCI_HP_IE_INFS |           // interface non-fatal error
                AHCI_HP_IE_OFS |            // overflow
                AHCI_HP_IE_IPMS |           // incorrect port multiplier
                AHCI_HP_IE_PRCS |           // phyready changed
                AHCI_HP_IE_PCS |            // port connect changed
                AHCI_HP_IE_UFS |            // unknown FIS
                AHCI_HP_IE_SDBS |           // set device enable bits
                AHCI_HP_IE_DSS |            // dma setup FIS
                AHCI_HP_IE_PSS |            // pio setup FIS
                AHCI_HP_IE_DHRS;            // device to host register FIS
    }

    AHCI_TRACE("Starting ports\n");
    port_start_all();

    // Acknowledge all IRQs
    AHCI_TRACE("Acknowledging top level IRQ\n");
    mmio_base->intr_status = mmio_base->intr_status;

    AHCI_TRACE("Enabling IRQ\n");

    // Enable interrupts overall
    mmio_base->host_ctl |= AHCI_HC_HC_IE;

    AHCI_TRACE("Rebase done\n");
}

void ahci_if_t::bios_handoff()
{
    // If BIOS handoff is not supported then return
    if ((mmio_base->cap2 & AHCI_HC_CAP2_BOH) == 0)
        return;

    // Request BIOS handoff
    mmio_base->bios_handoff =
            (mmio_base->bios_handoff & ~AHCI_HC_BOH_OOC) |
            AHCI_HC_BOH_OOS;

    atomic_barrier();
    while ((mmio_base->bios_handoff &
            (AHCI_HC_BOH_BOS | AHCI_HC_BOH_OOS)) !=
           AHCI_HC_BOH_OOS)
        thread_yield();
}

if_list_t ahci_if_factory_t::detect(void)
{
    unsigned start_at = ahci_count;

    if_list_t list = {
        ahci_devices + start_at,
        sizeof(*ahci_devices),
        0
    };

    pci_dev_iterator_t pci_iter;

    AHCI_TRACE("Enumerating PCI busses for AHCI...\n");
    //sleep(3000);

    if (!pci_enumerate_begin(
                &pci_iter,
                PCI_DEV_CLASS_STORAGE,
                PCI_SUBCLASS_STORAGE_SATA))
        return list;

    do {
        assert(pci_iter.dev_class == PCI_DEV_CLASS_STORAGE);
        assert(pci_iter.subclass == PCI_SUBCLASS_STORAGE_SATA);

        // Make sure it is an AHCI device
        if (pci_iter.config.prog_if != PCI_PROGIF_STORAGE_SATA_AHCI)
            continue;

        // Ignore controllers with AHCI base address not set
        if (pci_iter.config.base_addr[5] == 0)
            continue;

        AHCI_TRACE("Found AHCI Device BAR ht=%x %u/%u/%u d=%x s=%x:"
                   " %x %x %x %x %x %x\n",
                   pci_iter.config.header_type,
                   pci_iter.bus, pci_iter.slot, pci_iter.func,
                   pci_iter.dev_class, pci_iter.subclass,
                   pci_iter.config.base_addr[0],
                pci_iter.config.base_addr[1],
                pci_iter.config.base_addr[2],
                pci_iter.config.base_addr[3],
                pci_iter.config.base_addr[4],
                pci_iter.config.base_addr[5]);

        AHCI_TRACE("IRQ line=%d, IRQ pin=%d\n",
               pci_iter.config.irq_line,
               pci_iter.config.irq_pin);

        AHCI_TRACE("Initializing AHCI interface...\n");

        //sleep(3000);

        if (ahci_count < countof(ahci_devices)) {
            ahci_if_t *self = ahci_devices + ahci_count++;

            self->init(pci_iter);
        }
    } while (pci_enumerate_next(&pci_iter));

    list.count = ahci_count - start_at;

    return list;
}

//
// device registration

if_list_t ahci_if_t::detect_devices()
{
    unsigned start_at = ahci_drive_count;

    if_list_t list = {
        ahci_drives + start_at,
        sizeof(*ahci_drives),
        0
    };

    uint8_t port_num;
    for (unsigned ports_impl = ports_impl_mask; ports_impl;
         ports_impl &= ~(1U << port_num)) {
        port_num = bit_lsb_set(ports_impl);

        hba_port_t volatile *port = mmio_base->ports + port_num;

        if (port->sig == ahci_sig_t::SATA_SIG_ATA ||
                port->sig == ahci_sig_t::SATA_SIG_ATAPI) {
            ahci_dev_t *drive = ahci_drives + ahci_drive_count++;

            drive->init(this, port_num,
                        port->sig == ahci_sig_t::SATA_SIG_ATAPI);
        }
    }

    list.count = ahci_drive_count - start_at;

    return list;
}

void ahci_if_t::cleanup()
{
}

void ahci_dev_t::init(ahci_if_t *parent, unsigned dev_port, bool dev_is_atapi)
{
    assert(iface == nullptr);

    iface = parent;
    port = dev_port;
    is_atapi = dev_is_atapi;

    unique_ptr<ata_identify_t> identify = new ata_identify_t;

    if (io(identify, 1, 0, false, slot_op_t::identify) < 0)
        return;

    identify->fixup_strings();

    if (!dev_is_atapi) {
        iface->configure_48bit(dev_port, identify->support_ext48bit);

        iface->configure_ncq(port, identify->support_ncq,
                             identify->max_queue_minus1 + 1);

        iface->configure_fua(port, identify->support_fua_ext);
    }
}

void ahci_dev_t::cleanup()
{
}

struct ahci_blocking_io_t {
    ahci_blocking_io_t()
        : expect_count(0)
        , done_count(0)
        , err(0)
    {
        mutex_init(&lock);
        condvar_init(&done_cond);
    }

    ~ahci_blocking_io_t()
    {
        condvar_destroy(&done_cond);
        mutex_destroy(&lock);
    }

    mutex_t lock;
    condition_var_t done_cond;

    int expect_count;
    int done_count;
    int err;
};

static void ahci_async_complete(int error, uintptr_t arg)
{
    ahci_blocking_io_t *state = (ahci_blocking_io_t*)arg;

    mutex_lock_noyield(&state->lock);
    if (error)
        state->err = error;

    ++state->done_count;
    bool done = (state->done_count == state->expect_count);

    mutex_unlock(&state->lock);

    if (done)
        condvar_wake_one(&state->done_cond);
}

int ahci_dev_t::io(void *data, int64_t count,
                  uint64_t lba, bool fua, slot_op_t op)
{
    cpu_scoped_irq_disable intr_were_enabled;
    ahci_blocking_io_t block_state;

    mutex_lock(&block_state.lock);

    slot_request_t request;
    request.data = data;
    request.count = count;
    request.lba = lba;
    request.op = op;
    request.fua = fua;
    request.callback.callback = ahci_async_complete;
    request.callback.callback_arg = uintptr_t(&block_state);

    block_state.expect_count = iface->io(port, request);

    while (block_state.done_count != block_state.expect_count)
        condvar_wait(&block_state.done_cond, &block_state.lock);

    mutex_unlock(&block_state.lock);

    return block_state.err;
}

int64_t ahci_dev_t::read_blocks(
        void *data, int64_t count,
        uint64_t lba)
{
    return io(data, count, lba, false, slot_op_t::read);
}

int64_t ahci_dev_t::write_blocks(
        void const *data, int64_t count,
        uint64_t lba, bool fua)
{
    return io((void*)data, count, lba, fua, slot_op_t::write);
}

int64_t ahci_dev_t::trim_blocks(int64_t count, uint64_t lba)
{
    (void)count;
    (void)lba;
    return -int(errno_t::ENOSYS);
}

int ahci_dev_t::flush()
{
    cpu_scoped_irq_disable intr_were_enabled;
    ahci_blocking_io_t block_state;

    mutex_lock(&block_state.lock);

    iface->port_flush(port, ahci_async_complete, uintptr_t(&block_state));

    while (block_state.done_count != block_state.expect_count)
        condvar_wait(&block_state.done_cond, &block_state.lock);

    mutex_unlock(&block_state.lock);

    return block_state.err;
}

long ahci_dev_t::info(storage_dev_info_t key)
{
    switch (key) {
    case STORAGE_INFO_BLOCKSIZE:
        return iface->get_sector_size(port);

    case STORAGE_INFO_HAVE_TRIM:
        return 0;

    default:
        return 0;
    }
}
