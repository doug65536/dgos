#define STORAGE_IMPL
#include "device/ahci.h"
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

#define AHCI_DEBUG  1
#if AHCI_DEBUG
#define AHCI_TRACE(...) printdbg("ahci: " __VA_ARGS__)
#else
#define AHCI_TRACE(...) ((void)0)
#endif

enum ata_cmd_t {
    ATA_CMD_READ_PIO        = 0x20,
    ATA_CMD_READ_PIO_EXT    = 0x24,

    ATA_CMD_READ_DMA        = 0xC8,
    ATA_CMD_READ_DMA_EXT    = 0x25,

    ATA_CMD_WRITE_PIO       = 0x30,
    ATA_CMD_WRITE_PIO_EXT   = 0x34,

    ATA_CMD_WRITE_DMA       = 0xCA,
    ATA_CMD_WRITE_DMA_EXT   = 0x35,

    ATA_CMD_READ_DMA_NCQ    = 0x60,
    ATA_CMD_WRITE_DMA_NCQ   = 0x61,

    ATA_CMD_CACHE_FLUSH     = 0xE7,
    ATA_CMD_CACHE_FLUSH_EXT = 0xEA,

    ATA_CMD_PACKET          = 0xA0,
    ATA_CMD_IDENTIFY_PACKET = 0xA1,

    ATA_CMD_IDENTIFY        = 0xEC
};

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
    uint8_t command;

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
};

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
    uint8_t command;

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

enum ahci_sig_t {
    // SATA drive
    SATA_SIG_ATA    = (int32_t)0x00000101,

    // SATAPI drive
    SATA_SIG_ATAPI  = (int32_t)0xEB140101,

    // Enclosure management bridge
    SATA_SIG_SEMB   = (int32_t)0xC33C0101,

    // Port multiplier
    SATA_SIG_PM     = (int32_t)0x96690101
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

    // task file data
    uint32_t taskfile_data;

    // signature
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

    // vendor specific
    uint32_t vendor[4];
};

C_ASSERT(offsetof(hba_port_t, sata_act) == 0x34);
C_ASSERT(sizeof(hba_port_t) == 0x80);

// 0x00 - 0x2B, Generic Host Control
struct hba_host_ctl_t {
    // Host capability
    uint32_t cap;

    // Global host control
    uint32_t host_ctl;

    // Interrupt status (bitmask by port)
    uint32_t intr_status;

    // Port implemented (bitmask, 1=implemented)
    uint32_t ports_impl;

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
#define AHCI_HP_TFD_SBSY_BIT    7
#define AHCI_HP_TFD_SCS64_BIT   4
#define AHCI_HP_TFD_SDRQ_BIT    3
#define AHCI_HP_TFD_SCS21_BIT   1
#define AHCI_HP_TFD_SERR_BIT    0

#define AHCI_HP_TFD_ERR_BITS    8
#define AHCI_HP_TFD_ERR_MASK    ((1U<<AHCI_HP_TFD_ERR_BITS)-1)

#define AHCI_HP_TFD_ERR         (AHCI_HP_TFD_ERR_MASK<<AHCI_HP_TFD_ERR_BIT)
#define AHCI_HP_TFD_SERR        (1U<<AHCI_HP_TFD_SERR_BIT)

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
    char atapi_fis[16];
    char filler[0x80-0x50];

    // At offset 0x80
    hba_prdt_ent_t prdts[AHCI_CMD_TBL_ENT_MAX_PRD];
};

C_ASSERT(sizeof(hba_cmd_tbl_ent_t) == 1024);

typedef void (*async_callback_fn_t)(int error, int done, uintptr_t arg);

struct async_callback_t {
    async_callback_fn_t callback;
    uintptr_t callback_arg;
    int done;
    int error;
};

struct hba_port_info_t {
    hba_fis_t *fis;
    hba_cmd_hdr_t *cmd_hdr;
    hba_cmd_tbl_ent_t *cmd_tbl;
    async_callback_t callbacks[32];
    uint32_t is_atapi;
    uint32_t sector_size;
    uint32_t volatile cmd_issued;

    // Keep track of slot order
    spinlock_t lock;
    uint8_t issue_queue[32];
    uint8_t issue_head;
    uint8_t issue_tail;

    mutex_t slotalloc_lock;
    condition_var_t slotalloc_avail;
};

#define AHCI_PE_INTR_BIT    31
#define AHCI_PE_DBC_BIT     1
#define AHCI_PE_DBC_n(n)    ((n)-1)

struct ahci_if_factory_t : public storage_if_factory_t {
    ahci_if_factory_t() : storage_if_factory_t("ahci") {}
    virtual if_list_t detect(void);
};

ahci_if_factory_t ahci_factory;

// AHCI interface instance
struct ahci_if_t : public storage_if_base_t {
    STORAGE_IF_IMPL

    void init(pci_dev_iterator_t const &pci_iter);
    int supports_64bit();
    void release_slot(int port_num, int slot);
    void handle_port_irqs(int port_num);
    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    void port_stop(int port_num);
    void port_start(int port_num);
    void port_stop_all();
    void port_start_all();
    void rebase();
    void perform_detect(int port_num);

    void rw(int port_num, uint64_t lba,
            void *data, uint32_t count, int is_read,
            async_callback_fn_t callback,
            uintptr_t callback_arg);

    void bios_handoff();

    int acquire_slot(int port_num);

    void cmd_issue(int port_num, unsigned slot,
                   hba_cmd_cfis_t *cfis, void *atapi_fis,
                   size_t fis_size, hba_prdt_ent_t *prdts,
                   size_t ranges_count, int is_read,
                   async_callback_fn_t callback, int done,
                   uintptr_t callback_arg);

    pci_config_hdr_t config;

    // Linear addresses
    hba_host_ctl_t volatile *mmio_base;

    void *buffers;

    hba_port_info_t port_info[32];

    int use_msi;
    pci_irq_range_t irq_range;
    int ports_impl;
    int num_cmd_slots;
    int use_ncq;
    int use_64;
};

// Drive
struct ahci_dev_t : public storage_dev_base_t {
    STORAGE_DEV_IMPL

    int io(void *data, uint64_t count,
           uint64_t lba, int is_read);

    ahci_if_t *if_;
    int port;
    int is_atapi;
};

#define AHCI_MAX_DEVICES    16
static ahci_if_t ahci_devices[AHCI_MAX_DEVICES];
static unsigned ahci_count;

#define AHCI_MAX_DRIVES    64
static ahci_dev_t ahci_drives[AHCI_MAX_DRIVES];
static unsigned ahci_drive_count;

int ahci_if_t::supports_64bit()
{
    return (mmio_base->cap & AHCI_HC_CAP_S64A) != 0;
}

// Must be holding port spinlock
void ahci_if_t::release_slot(int port_num, int slot)
{
    assert(slot >= 0);

    hba_port_info_t *pi = port_info + port_num;

    mutex_lock_noyield(&pi->slotalloc_lock);

    // Make sure it was really acquired
    assert(pi->cmd_issued & (1U<<slot));

    if (!use_ncq) {
        // Make sure this was really the oldest command
        assert(pi->issue_queue[pi->issue_tail] == slot);
        // Advance queue tail
        pi->issue_tail = (pi->issue_tail + 1) & 31;
    }

    // Mark slot as not in use
    atomic_and(&pi->cmd_issued, ~(1U<<slot));

    mutex_unlock(&pi->slotalloc_lock);
    condvar_wake_one(&pi->slotalloc_avail);
}

// Acquire slot that is not in use
// Returns -1 if all slots are in use
// Must be holding port spinlock
int ahci_if_t::acquire_slot(int port_num)
{
    hba_port_info_t *pi = port_info + port_num;

    for (;;) {
        // Build bitmask of slots in use
        uint32_t busy_mask = pi->cmd_issued;

        // If every slot is busy
        if (busy_mask == 0xFFFFFFFF ||
                (num_cmd_slots < 32 &&
                busy_mask == ((1U<<num_cmd_slots)-1)))
            return -1;

        // Find first zero
        int slot = bit_lsb_set_32(~busy_mask);

        uint32_t old_busy = pi->cmd_issued;
        if ((old_busy & (1U<<slot)) == 0) {
            if (old_busy == atomic_cmpxchg(
                        &pi->cmd_issued, old_busy,
                        old_busy | (1U<<slot))) {
                if (!use_ncq) {
                    // Write slot number to issue queue
                    pi->issue_queue[pi->issue_head] = slot;
                    pi->issue_head = (pi->issue_head + 1) & 31;
                }

                return slot;
            }
        }
    }
}

void ahci_if_t::handle_port_irqs(int port_num)
{
    hba_host_ctl_t volatile *hba = mmio_base;
    hba_port_t volatile *port = hba->ports + port_num;
    hba_port_info_t *pi = port_info + port_num;

    async_callback_t pending_callbacks[32];
    unsigned callback_count = 0;

    spinlock_lock_noyield(&pi->lock);

    // Read command slot interrupt status
    int slot;

    if (use_ncq) {
        for (uint32_t done_slots = pi->cmd_issued &
             (pi->cmd_issued ^ port->sata_act);
             done_slots; done_slots &= ~(1U << slot)) {
            slot = bit_lsb_set_32(done_slots);

            // FIXME: check error
            int error = (port->sata_err != 0);
            assert(error == 0);

            pi->callbacks[slot].error = error;

            // Invoke completion callback
            assert(pi->callbacks[slot].callback);
            assert(callback_count < countof(pending_callbacks));
            pending_callbacks[callback_count++] = pi->callbacks[slot];
            pi->callbacks[slot].callback = 0;
            pi->callbacks[slot].callback_arg = 0;
            pi->callbacks[slot].done = 0;
            pi->callbacks[slot].error = 0;

            release_slot(port_num, slot);
        }
    } else {
        slot = pi->issue_queue[pi->issue_tail];

        int error = 0;

        if (port->sata_err != 0)
            error = 1;

        if ((port->taskfile_data & AHCI_HP_TFD_ERR) ||
                (port->taskfile_data & AHCI_HP_TFD_SERR))
            error = 2;

        if (error != 0) {
            AHCI_TRACE("Error %d on interface=%zu port=%zu\n",
                       error, this - ahci_devices,
                       pi - port_info);
        }

        pi->callbacks[slot].error = error;

        // Invoke completion callback
        assert(pi->callbacks[slot].callback);
        assert(callback_count < countof(pending_callbacks));
        pending_callbacks[callback_count++] = pi->callbacks[slot];
        pi->callbacks[slot].callback = 0;
        pi->callbacks[slot].callback_arg = 0;
        pi->callbacks[slot].done = 0;
        pi->callbacks[slot].error = 0;

        release_slot(port_num, slot);
    }

    // Acknowledge slot interrupt
    port->intr_status |= port->intr_status;

    spinlock_unlock_noirq(&pi->lock);

    // Make all callbacks outside lock
    for (unsigned i = 0; i < callback_count; ++i) {
        async_callback_t *callback = pending_callbacks + i;
        if (callback->callback) {
            callback->callback(callback->error,
                               callback->done,
                               callback->callback_arg);
        }
    }
}

isr_context_t *ahci_if_t::irq_handler(int irq, isr_context_t *ctx)
{
    for (unsigned i = 0; i < ahci_count; ++i) {
        ahci_if_t *dev = ahci_devices + i;

        int irq_offset = irq - dev->irq_range.base;

        if (irq_offset < 0 || irq_offset > dev->irq_range.count)
            continue;

        // Call callback on every port that has an interrupt pending
        int port;
        for (uint32_t intr_status = dev->mmio_base->intr_status;
             intr_status != 0; intr_status &= ~(1U<<port)) {
            // Look through each port
            port = bit_lsb_set_32(intr_status);

            dev->handle_port_irqs(port);

            // Acknowledge the interrupt on the port
            dev->mmio_base->intr_status = (1U<<port);
        }
    }

    return ctx;
}

void ahci_if_t::port_stop(int port_num)
{
    hba_port_t volatile *port = mmio_base->ports + port_num;

    // Clear start bit
    // Clear FIS receive enable bit
    port->cmd &= ~(AHCI_HP_CMD_ST | AHCI_HP_CMD_FRE);

    atomic_barrier();

    // Wait until there is not a command running,
    // and there is not a FIS receive running
    while (port->cmd & (AHCI_HP_CMD_CR | AHCI_HP_CMD_FR))
        thread_yield();
}

void ahci_if_t::port_start(int port_num)
{
    hba_port_t volatile *port = mmio_base->ports + port_num;

    // Wait until there is not a command running, and
    // there is not a FIS receive running
    while (port->cmd & (AHCI_HP_CMD_CR | AHCI_HP_CMD_FR))
        thread_yield();

    // Set start and FIS receive enable bit
    port->cmd |= AHCI_HP_CMD_ST | AHCI_HP_CMD_FRE;
}

// Stop each implemented port
void ahci_if_t::port_stop_all()
{
    int port;
    for (uint32_t impl = ports_impl;
         impl != 0; impl &= ~(1U<<port)) {
        port = bit_lsb_set_32(impl);
        port_stop(port);
    }
}

// Start each implemented port
void ahci_if_t::port_start_all()
{
    int port;
    for (uint32_t impl = ports_impl;
         impl != 0; impl &= ~(1U<<port)) {
        port = bit_lsb_set_32(impl);
        port_start(port);
    }
}

void ahci_if_t::cmd_issue(
        int port_num, unsigned slot,
        hba_cmd_cfis_t *cfis, void *atapi_fis,
        size_t fis_size, hba_prdt_ent_t *prdts, size_t ranges_count,
        int is_read, async_callback_fn_t callback,
        int done, uintptr_t callback_arg)
{
    hba_port_info_t *pi = port_info + port_num;
    hba_port_t volatile *port = mmio_base->ports + port_num;

    hba_cmd_hdr_t *cmd_hdr = pi->cmd_hdr + slot;
    hba_cmd_tbl_ent_t *cmd_tbl_ent = pi->cmd_tbl + slot;

    spinlock_lock_noirq(&pi->lock);

    memcpy(cmd_tbl_ent->prdts, prdts, sizeof(cmd_tbl_ent->prdts));

    memcpy(&cmd_tbl_ent->cfis, cfis, sizeof(*cfis));

    if (atapi_fis) {
        memcpy(cmd_tbl_ent->atapi_fis, atapi_fis,
               sizeof(cmd_tbl_ent->atapi_fis));
    }

    cmd_hdr->hdr = AHCI_CH_LEN_n(fis_size >> 2) |
            (!is_read ? AHCI_CH_WR : 0) |
            (atapi_fis ? AHCI_CH_ATAPI : 0);
    cmd_hdr->prdbc = 0;
    cmd_hdr->prdtl = ranges_count;

    pi->callbacks[slot].callback = callback;
    pi->callbacks[slot].callback_arg = callback_arg;
    pi->callbacks[slot].done = done;
    pi->callbacks[slot].error = 0;

    atomic_barrier();

    if (use_ncq)
        port->sata_act = (1U<<slot);

    atomic_barrier();
    port->cmd_issue = (1U<<slot);

    spinlock_unlock_noirq(&pi->lock);
}

void ahci_if_t::init(const pci_dev_iterator_t &pci_iter)
{
    config = pci_iter.config;

    mmio_base = (hba_host_ctl_t*)
            mmap((void*)(uintptr_t)pci_iter.config.base_addr[5],
            0x1100, PROT_READ | PROT_WRITE,
            MAP_PHYSICAL, -1, 0);

    AHCI_TRACE("Performing BIOS handoff\n");
    bios_handoff();

    // Cache implemented port bitmask
    ports_impl = mmio_base->ports_impl;

    // Cache number of command slots per port
    num_cmd_slots = 1 + ((mmio_base->cap >>
                          AHCI_HC_CAP_NCS_BIT) &
                          AHCI_HC_CAP_NCS_MASK);

    use_64 = !!(mmio_base->cap & AHCI_HC_CAP_S64A);
    //use_ncq = !!(mmio_base->cap & AHCI_HC_CAP_SNCQ);
    use_ncq = 0;

    rebase();

    // Assume legacy IRQ pin usage until MSI succeeds
    irq_range.base = pci_iter.config.irq_line;
    irq_range.count = 1;

    // Try to use MSI IRQ
    use_msi = pci_set_msi_irq(
                pci_iter.bus, pci_iter.slot, pci_iter.func,
                &irq_range, 1, 1, 1,
                &ahci_if_t::irq_handler);

    if (!use_msi) {
        // Fall back to pin based IRQ
        irq_hook(irq_range.base, &ahci_if_t::irq_handler);
        irq_setmask(irq_range.base, 1);
    }
}

void ahci_if_t::rw(int port_num, uint64_t lba,
                   void *data, uint32_t count, int is_read,
                   async_callback_fn_t callback,
                   uintptr_t callback_arg)
{
    mmphysrange_t ranges[AHCI_CMD_TBL_ENT_MAX_PRD];
    size_t ranges_count;

    hba_prdt_ent_t prdts[AHCI_CMD_TBL_ENT_MAX_PRD];

    hba_port_info_t *pi = port_info + port_num;

    do {
        ranges_count = mphysranges(ranges, countof(ranges),
                                   data, count * pi->sector_size,
                                   4<<20);

        size_t transferred = 0;

        memset(prdts, 0, sizeof(prdts));
        for (size_t i = 0; i < ranges_count; ++i) {
            prdts[i].dba = ranges[i].physaddr;
            prdts[i].dbc_intr = AHCI_PE_DBC_n(ranges[i].size);

            transferred += ranges[i].size;
        }

        size_t transferred_blocks = transferred / pi->sector_size;

        // Wait for a slot
        mutex_lock(&pi->slotalloc_lock);
        int slot;
        for (;;) {
            slot = acquire_slot(port_num);
            if (slot >= 0)
                break;

            condvar_wait(&pi->slotalloc_avail, &pi->slotalloc_lock);
        }
        mutex_unlock(&pi->slotalloc_lock);

        hba_cmd_cfis_t cfis;
        size_t fis_size;

        memset(&cfis, 0, sizeof(cfis));

        uint8_t atapifis[16];

        if (pi->is_atapi) {
            atapifis[0] = 0xA8;
            atapifis[1] = 0;
            atapifis[2] = (lba >> 24) & 0xFF;
            atapifis[3] = (lba >> 16) & 0xFF;
            atapifis[4] = (lba >>  8) & 0xFF;
            atapifis[5] = (lba >>  0) & 0xFF;
            atapifis[6] = (transferred_blocks >> 24) & 0xFF;
            atapifis[7] = (transferred_blocks >> 16) & 0xFF;
            atapifis[8] = (transferred_blocks >>  8) & 0xFF;
            atapifis[9] = (transferred_blocks >>  0) & 0xFF;
            atapifis[10] = 0;
            atapifis[11] = 0;

            atapifis[12] = 0;
            atapifis[13] = 0;
            atapifis[14] = 0;
            atapifis[15] = 0;

            fis_size = sizeof(cfis.h2d);

            cfis.h2d.fis_type = FIS_TYPE_REG_H2D;
            cfis.h2d.ctl = AHCI_FIS_CTL_CMD;
            cfis.h2d.command = ATA_CMD_PACKET;
            cfis.h2d.feature_lo = 1;    // DMA
            cfis.h2d.lba0 = 0;
            cfis.h2d.lba2 = 0;
            cfis.h2d.lba3 = 2048 >> 8;
            cfis.h2d.lba5 = 0;
            cfis.h2d.count = 0;

            // LBA
            cfis.d2h.device = 0;
        } else if (use_ncq) {
            fis_size = sizeof(cfis.ncq);

            cfis.ncq.fis_type = FIS_TYPE_REG_H2D;
            cfis.ncq.ctl = AHCI_FIS_CTL_CMD;
            cfis.ncq.command = is_read
                    ? ATA_CMD_READ_DMA_NCQ
                    : ATA_CMD_WRITE_DMA_NCQ;
            cfis.ncq.lba0 = lba & 0xFFFF;
            cfis.ncq.lba2 = (lba >> 16) & 0xFF;
            cfis.ncq.lba3 = (lba >> 24) & 0xFFFF;
            cfis.ncq.lba5 = (lba >> 40);
            cfis.ncq.count_lo = transferred_blocks & 0xFF;
            cfis.ncq.count_hi = (transferred_blocks >> 8) & 0xFF;
            cfis.ncq.tag = AHCI_FIS_TAG_TAG_n(slot);
            cfis.ncq.fua = AHCI_FIS_FUA_LBA |
                    (!is_read ? AHCI_FIS_FUA_FUA : 0);
            cfis.ncq.prio = 0;
            cfis.ncq.aux = 0;
        } else {
            fis_size = sizeof(cfis.h2d);

            cfis.h2d.fis_type = FIS_TYPE_REG_H2D;
            cfis.h2d.ctl = AHCI_FIS_CTL_CMD;
            cfis.h2d.command = is_read
                    ? ATA_CMD_READ_DMA_EXT
                    : ATA_CMD_WRITE_DMA_EXT;
            assert(lba < 0x1000000000000L);
            cfis.h2d.lba0 = lba & 0xFF;
            cfis.h2d.lba1 = (lba >> 8) & 0xFF;
            cfis.h2d.lba2 = (lba >> 16) & 0xFF;
            cfis.h2d.lba3 = (lba >> 24) & 0xFF;
            cfis.h2d.lba4 = (lba >> 32) & 0xFF;
            cfis.h2d.lba5 = (lba >> 40);
            cfis.h2d.count = transferred_blocks;
            cfis.h2d.feature_lo = 1;

            // LBA
            cfis.d2h.device = AHCI_FIS_FUA_LBA;
        }

        atomic_barrier();

        cmd_issue(port_num, (unsigned)slot,
                  &cfis, pi->is_atapi ? atapifis : 0,
                  fis_size, prdts, ranges_count, is_read,
                  callback, count == transferred_blocks,
                  callback_arg);

        data = (char*)data + transferred;
        lba += transferred_blocks;
        count -= transferred_blocks;
    } while (count > 0);
}

// The command engine must be stopped before calling ahci_perform_detect
void ahci_if_t::perform_detect(int port_num)
{
    hba_port_t volatile *port = mmio_base->ports + port_num;

    // Enable FIS receive
    port->cmd |= AHCI_HP_CMD_FRE;

    // Put port into INIT state
    port->sata_ctl = (port->sata_ctl & ~AHCI_HP_SC_DET) |
            AHCI_HP_SC_DET_n(AHCI_HP_SC_DET_INIT);

    // Wait 3x the documented minimum
    usleep(3000);

    // Put port into normal operation
    port->sata_ctl = (port->sata_ctl & ~AHCI_HP_SC_DET) |
            AHCI_HP_SC_DET_n(0);

    // Clear FIS receive bit
    port->cmd &= ~AHCI_HP_CMD_FRE;

    // Acknowledge FIS receive interrupt
    port->intr_status |= AHCI_HP_IS_DHRS;
}

void ahci_if_t::rebase()
{
    AHCI_TRACE("Stopping all ports\n");
    // Stop all ports
    port_stop_all();

    int support_64bit = supports_64bit();

    int addr_type = support_64bit
            ? MAP_POPULATE
            : MAP_POPULATE | MAP_32BIT
            ;

    AHCI_TRACE("64 bit support: %d\n", support_64bit);

    // Loop through the implemented ports

    // Initial "slot busy" mask marks unimplemented slots
    // as permanently busy
    uint32_t init_busy_mask = (num_cmd_slots == 32)
            ? 0
            : ((uint32_t)~0<<num_cmd_slots);

    for (uint32_t port_num = 0; port_num < 32; ++port_num) {
        if (!(ports_impl & (1U<<port_num)))
            continue;

        AHCI_TRACE("Initializing AHCI device port %d\n", port_num);

        hba_port_t volatile *port = mmio_base->ports + port_num;
        hba_port_info_t *pi = port_info + port_num;

        mutex_init(&pi->slotalloc_lock);
        condvar_init(&pi->slotalloc_avail);

        AHCI_TRACE("Performing detection, port %d\n", port_num);
        perform_detect(port_num);

        // FIXME: do IDENTIFY instead of assuming 2KB/512B
        if (port->sig == SATA_SIG_ATAPI) {
            pi->is_atapi = 1;
            pi->sector_size = 2048;
            port->cmd |= AHCI_HP_CMD_ATAPI;
        } else if (port->sig == SATA_SIG_ATA) {
            pi->is_atapi = 0;
            pi->sector_size = 512;
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
        // (Workaround initially active slot on QEMU)
        pi->cmd_issued = init_busy_mask;

        AHCI_TRACE("Setting cmd/FIS buffer addresses\n");

        atomic_barrier();
        port->cmd_list_base = mphysaddr(cmd_hdr);
        port->fis_base = mphysaddr((void*)fis);
        atomic_barrier();

        // Set command table base addresses (physical)
        for (int slot = 0; slot < num_cmd_slots; ++slot)
            cmd_hdr[slot].ctba = mphysaddr(cmd_tbl + slot);

        // Acknowledging interrupts
        port->intr_status = port->intr_status;

        AHCI_TRACE("Unmasking interrupts\n");

        port->intr_en = AHCI_HP_IE_TFES |
                AHCI_HP_IE_HBFS |
                AHCI_HP_IE_HBDS |
                AHCI_HP_IE_IFS |
                AHCI_HP_IE_INFS |
                AHCI_HP_IE_OFS |
                AHCI_HP_IE_IPMS |
                AHCI_HP_IE_PRCS |
                AHCI_HP_IE_PCS |
                AHCI_HP_IE_UFS |
                AHCI_HP_IE_SDBS |
                AHCI_HP_IE_DSS |
                AHCI_HP_IE_PSS |
                AHCI_HP_IE_DHRS;
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

        AHCI_TRACE("Finding next AHCI device...\n");
        //sleep(3000);

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

    for (int port_num = 0; port_num < 32; ++port_num) {
        if (!(ports_impl & (1U<<port_num)))
            continue;

        hba_port_t volatile *port = mmio_base->ports + port_num;

        if (port->sig == SATA_SIG_ATA || port->sig == SATA_SIG_ATAPI) {
            ahci_dev_t *drive = ahci_drives + ahci_drive_count++;
            drive->if_ = this;
            drive->port = port_num;
            drive->is_atapi = (port->sig == SATA_SIG_ATAPI);
        }
    }

    list.count = ahci_drive_count - start_at;

    return list;
}

void ahci_if_t::cleanup()
{
}

void ahci_dev_t::cleanup()
{
}

struct ahci_blocking_io_t {
    mutex_t lock;
    condition_var_t done_cond;

    int done;
    int err;
    uint64_t lba;
};

static void ahci_async_complete(int error,
                                int done,
                                uintptr_t arg)
{
    ahci_blocking_io_t *state = (ahci_blocking_io_t*)arg;

    mutex_lock_noyield(&state->lock);
    if (error)
        state->err = error;
    state->done = done;
    mutex_unlock(&state->lock);
    if (done)
        condvar_wake_one(&state->done_cond);
}

int ahci_dev_t::io(void *data, uint64_t count,
                  uint64_t lba, int is_read)
{
    ahci_blocking_io_t block_state;
    memset(&block_state, 0, sizeof(block_state));

    mutex_init(&block_state.lock);
    condvar_init(&block_state.done_cond);

    int intr_were_enabled = cpu_irq_disable();

    mutex_lock(&block_state.lock);

    block_state.err = 0;
    block_state.lba = lba;

    if_->rw(port, lba, data, count, is_read,
       ahci_async_complete, (uintptr_t)&block_state);

    while (!block_state.done)
        condvar_wait(&block_state.done_cond, &block_state.lock);

    mutex_unlock(&block_state.lock);

    condvar_destroy(&block_state.done_cond);
    mutex_destroy(&block_state.lock);

    cpu_irq_toggle(intr_were_enabled);

    return block_state.err;
}

int ahci_dev_t::read_blocks(
        void *data, uint64_t count,
        uint64_t lba)
{
    return io(data, count, lba, 1);
}

int ahci_dev_t::write_blocks(
        void const *data, uint64_t count,
        uint64_t lba)
{
    return io((void*)data, count, lba, 0);
}

int ahci_dev_t::flush()
{
    return 0;
}

long ahci_dev_t::info(storage_dev_info_t key)
{
    switch (key) {
    case STORAGE_INFO_BLOCKSIZE:
        return if_->port_info[port].sector_size;

    default:
        return 0;
    }
}
