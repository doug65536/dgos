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

typedef struct ahci_if_t ahci_if_t;
DECLARE_storage_if_DEVICE(ahci);
DECLARE_storage_dev_DEVICE(ahci);

typedef enum ata_cmd_t {
    ATA_CMD_READ_PIO        = 0x20,
    ATA_CMD_READ_PIO_EXT    = 0x24,
    ATA_CMD_READ_DMA        = 0xC8,
    ATA_CMD_READ_DMA_EXT    = 0x25,
    ATA_CMD_WRITE_PIO       = 0x30,
    ATA_CMD_WRITE_PIO_EXT   = 0x34,
    ATA_CMD_WRITE_DMA       = 0xCA,
    ATA_CMD_WRITE_DMA_EXT   = 0x35,
    ATA_CMD_CACHE_FLUSH     = 0xE7,
    ATA_CMD_CACHE_FLUSH_EXT = 0xEA,
    ATA_CMD_PACKET          = 0xA0,
    ATA_CMD_IDENTIFY_PACKET = 0xA1,
    ATA_CMD_IDENTIFY        = 0xEC
} ata_cmd_t;

typedef enum ahci_fis_type_t {
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
} ahci_fis_type_t;

typedef struct ahci_fis_h2d_t {
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
} ahci_fis_h2d_t;

#define AHCI_FIS_CTL_PORTMUX_BIT    0
#define AHCI_FIS_CTL_PORTMUX_BITS   4
#define AHCI_FIS_CTL_CMD_BIT        7

#define AHCI_FIS_CTL_PORTMUX_MASK   ((1U<<AHCI_FIS_CTL_PORTMUX_BITS)-1)
#define AHCI_FIS_CTL_PORTMUX_n(n)   ((n)<<AHCI_FIS_CTL_PORTMUX_BITS)

#define AHCI_FIS_CTL_CMD            (1U<<AHCI_FIS_CTL_CMD_BIT)
#define AHCI_FIS_CTL_CTL            (0U<<AHCI_FIS_CTL_CMD_BIT)

typedef struct ahci_fis_d2h_t
{
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
} ahci_fis_d2h_t;

#define AHCI_FIS_CTL_INTR_BIT   6
#define AHCI_FIS_CTL_INTR       (1U<<AHCI_FIS_CTL_INTR_BIT)

typedef struct ahci_pio_setup_t {
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
} ahci_pio_setup_t;

typedef struct ahci_dma_setup_t {
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
} ahci_dma_setup_t;

C_ASSERT(sizeof(ahci_dma_setup_t) == 0x1C);

#define AHCI_FIS_CTL_DIR_BIT    5
#define AHCI_FIS_CTL_AUTO_BIT   7

#define AHCI_FIS_CTL_DIR        (1U<<AHCI_FIS_CTL_DIR_BIT)
#define AHCI_FIS_AUTO_DIR       (1U<<AHCI_FIS_CTL_AUTO_BIT)

// MMIO

typedef enum ahci_sig_t {
    // SATA drive
    SATA_SIG_ATA    = (int32_t)0x00000101,

    // SATAPI drive
    SATA_SIG_ATAPI  = (int32_t)0xEB140101,

    // Enclosure management bridge
    SATA_SIG_SEMB   = (int32_t)0xC33C0101,

    // Port multiplier
    SATA_SIG_PM     = (int32_t)0x96690101
} ahci_sig_t;

C_ASSERT(sizeof(ahci_sig_t) == 4);

typedef struct hba_port_t {
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
} hba_port_t;

C_ASSERT(offsetof(hba_port_t, sata_act) == 0x34);
C_ASSERT(sizeof(hba_port_t) == 0x80);

// 0x00 - 0x2B, Generic Host Control
typedef struct hba_host_ctl_t
{
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
} hba_host_ctl_t;

C_ASSERT(offsetof(hba_host_ctl_t, rsv) == 0x2C);
C_ASSERT(offsetof(hba_host_ctl_t, ports) == 0x100);

//
// hba_host_ctl::cap

// 64 bit capable
#define AHCI_HC_CAP_S64A_BIT    31

// Supports Native Command Queuing
#define AHCI_HC_CAP_SNCQ_BIT    30

// Supports SNotification Register
#define AHCI_HC_CAP_SSNTF_BIT   29

// Supports mechanical presence switch
#define AHCI_HC_CAP_SMPS_BIT    28

// Supports Staggered Spinup
#define AHCI_HC_CAP_SSS_BIT     27

// Supports Aggressive Link Power management
#define AHCI_HC_CAP_SALP_BIT    26

// Supports Activity LED
#define AHCI_HC_CAP_SAL_BIT     25

// Supports Command List Override
#define AHCI_HC_CAP_SCLO_BIT    24

// Interface speed support
#define AHCI_HC_CAP_ISS_BIT     20

// Supports AHCI Mode only
#define AHCI_HC_CAP_SAM_BIT     18

// Supports Port Multiplier
#define AHCI_HC_CAP_SPM_BIT     17

// FIS Based Switching Supported
#define AHCI_HC_CAP_FBSS_BIT    16

// PIO Multiple DRQ Block
#define AHCI_HC_CAP_PMD_BIT     15

// Slumber State Capable
#define AHCI_HC_CAP_SSC_BIT     14

// Partial State Capable
#define AHCI_HC_CAP_PSC_BIT     13

// Number of Command Slots
#define AHCI_HC_CAP_NCS_BIT     8

// Command Completetion Coalescing Supported
#define AHCI_HC_CAP_CCCS_BIT    7

// Enclosure Management Supported
#define AHCI_HC_CAP_EMS_BIT     6

// Supports eXternal SATA
#define AHCI_HC_CAP_SXS_BIT     5

// Number of Ports
#define AHCI_HC_CAP_NP_BIT      0

#define AHCI_HC_CAP_NCS_BITS    5
#define AHCI_HC_CAP_ISS_BITS    4
#define AHCI_HC_CAP_NP_BITS     5

#define AHCI_HC_CAP_S64A        (1U<<AHCI_HC_CAP_S64A_BIT)
#define AHCI_HC_CAP_SNCQ        (1U<<AHCI_HC_CAP_SNCQ_BIT)
#define AHCI_HC_CAP_SSNTF       (1U<<AHCI_HC_CAP_SSNTF_BIT)
#define AHCI_HC_CAP_SMPS        (1U<<AHCI_HC_CAP_SMPS_BIT)
#define AHCI_HC_CAP_SSS         (1U<<AHCI_HC_CAP_SSS_BIT)
#define AHCI_HC_CAP_SALP        (1U<<AHCI_HC_CAP_SALP_BIT)
#define AHCI_HC_CAP_SAL         (1U<<AHCI_HC_CAP_SAL_BIT)
#define AHCI_HC_CAP_SCLO        (1U<<AHCI_HC_CAP_SCLO_BIT)
#define AHCI_HC_CAP_ISS         (1U<<AHCI_HC_CAP_ISS_BIT)
#define AHCI_HC_CAP_SAM         (1U<<AHCI_HC_CAP_SAM_BIT)
#define AHCI_HC_CAP_SPM         (1U<<AHCI_HC_CAP_SPM_BIT)
#define AHCI_HC_CAP_FBSS        (1U<<AHCI_HC_CAP_FBSS_BIT)
#define AHCI_HC_CAP_PMD         (1U<<AHCI_HC_CAP_PMD_BIT)
#define AHCI_HC_CAP_SSC         (1U<<AHCI_HC_CAP_SSC_BIT)
#define AHCI_HC_CAP_PSC         (1U<<AHCI_HC_CAP_PSC_BIT)
#define AHCI_HC_CAP_NCS         (1U<<AHCI_HC_CAP_NCS_BIT)
#define AHCI_HC_CAP_CCCS        (1U<<AHCI_HC_CAP_CCCS_BIT)
#define AHCI_HC_CAP_EMS         (1U<<AHCI_HC_CAP_EMS_BIT)
#define AHCI_HC_CAP_SXS         (1U<<AHCI_HC_CAP_SXS_BIT)
#define AHCI_HC_CAP_NP          (1U<<AHCI_HC_CAP_NP_BIT)

#define AHCI_HC_CAP_ISS_MASK    ((1U<<AHCI_HC_CAP_ISS_BITS)-1)
#define AHCI_HC_CAP_NCS_MASK    ((1U<<AHCI_HC_CAP_NCS_BITS)-1)
#define AHCI_HC_CAP_NP_MASK     ((1U<<AHCI_HC_CAP_NP_BITS)-1)

//
// hba_host_ctl::cap2

// DevSleep Entrance From Slumber Only
#define AHCI_HC_CAP2_DESO_BIT   5

// Supports Aggressive Device sleep Management
#define AHCI_HC_CAP2_SADM_BIT   4

// Supports Device Sleep
#define AHCI_HC_CAP2_SDS_BIT    3

// Automatic Partial Slumber Transitions
#define AHCI_HC_CAP2_APST_BIT   2

// NVMHCI Present
#define AHCI_HC_CAP2_NVMP_BIT   1

// BIOS/OS Handoff
#define AHCI_HC_CAP2_BOH_BIT    0

#define AHCI_HC_CAP2_DESO       (1U<<AHCI_HC_CAP2_DESO_BIT)
#define AHCI_HC_CAP2_SADM       (1U<<AHCI_HC_CAP2_SADM_BIT)
#define AHCI_HC_CAP2_SDS        (1U<<AHCI_HC_CAP2_SDS_BIT)
#define AHCI_HC_CAP2_APST       (1U<<AHCI_HC_CAP2_APST_BIT)
#define AHCI_HC_CAP2_NVMP       (1U<<AHCI_HC_CAP2_NVMP_BIT)
#define AHCI_HC_CAP2_BOH        (1U<<AHCI_HC_CAP2_BOH_BIT)

//
// hba_host_ctl::bios_handoff

// BIOS Busy
#define AHCI_HC_BOH_BB_BIT      4

// OS Ownership Change
#define AHCI_HC_BOH_OOC_BIT     3

// SMI on OS Ownership Change
#define AHCI_HC_BOH_SOOE_BIT    2

// OS Owned Semaphore
#define AHCI_HC_BOH_OOS_BIT     1

// BIOS Owned Semaphore
#define AHCI_HC_BOH_BOS_BIT     0

#define AHCI_HC_BOH_BB          (1U<<AHCI_HC_BOH_BB_BIT)
#define AHCI_HC_BOH_OOC         (1U<<AHCI_HC_BOH_OOC_BIT)
#define AHCI_HC_BOH_SOOE        (1U<<AHCI_HC_BOH_SOOE_BIT)
#define AHCI_HC_BOH_OOS         (1U<<AHCI_HC_BOH_OOS_BIT)
#define AHCI_HC_BOH_BOS         (1U<<AHCI_HC_BOH_BOS_BIT)

//
// hba_host_ctl::host_ctl

// AHCI Enable
#define AHCI_HC_HC_AE_BIT       31

// MSI Revert to Single Message
#define AHCI_HC_HC_MRSM_BIT     2

// Interrupt Enable
#define AHCI_HC_HC_IE_BIT       1

// HBA Reset
#define AHCI_HC_HC_HR_BIT       0

#define AHCI_HC_HC_AE           (1U<<AHCI_HC_HC_AE_BIT)
#define AHCI_HC_HC_MRSM         (1U<<AHCI_HC_HC_MRSM_BIT)
#define AHCI_HC_HC_IE           (1U<<AHCI_HC_HC_IE_BIT)
#define AHCI_HC_HC_HR           (1U<<AHCI_HC_HC_HR_BIT)

//
// hba_port_t::intr_status

// Cold Port Detect Status
#define AHCI_HP_IS_CPDS_BIT     31

// Task File Error Status
#define AHCI_HP_IS_TFES_BIT     30

// Host Bus Fatal Error Status
#define AHCI_HP_IS_HBFS_BIT     29

// Host Bus Data Error Status
#define AHCI_HP_IS_HBDS_BIT     28

// Interface Fatal Error Status
#define AHCI_HP_IS_IFS_BIT      27

// Interface Non-fatal Error Status
#define AHCI_HP_IS_INFS_BIT     26

// Overflow Status
#define AHCI_HP_IS_OFS_BIT      24

// Incorrect Port Multiplier Status
#define AHCI_HP_IS_IPMS_BIT     23

// PhyRdy Change Status
#define AHCI_HP_IS_PRCS_BIT     22

// Device Mechanical Presence Status
#define AHCI_HP_IS_DMPS_BIT     7

// Port Connect Change Status
#define AHCI_HP_IS_PCS_BIT      6

// Descriptor Processed Status
#define AHCI_HP_IS_DPS_BIT      5

// Unknown FIS Interrupt
#define AHCI_HP_IS_UFS_BIT      4

// Set Device Bits Interrupt
#define AHCI_HP_IS_SDBS_BIT     3

// DMA Setup FIS Interrupt
#define AHCI_HP_IS_DSS_BIT      2

// PIO Setup FIS Interrupt
#define AHCI_HP_IS_PSS_BIT      1

// Device to Host Register FIS Interrupt
#define AHCI_HP_IS_DHRS_BIT     0

#define AHCI_HP_IS_CPDS         (1U<<AHCI_HP_IS_CPDS_BIT)
#define AHCI_HP_IS_TFES         (1U<<AHCI_HP_IS_TFES_BIT)
#define AHCI_HP_IS_HBFS         (1U<<AHCI_HP_IS_HBFS_BIT)
#define AHCI_HP_IS_HBDS         (1U<<AHCI_HP_IS_HBDS_BIT)
#define AHCI_HP_IS_IFS          (1U<<AHCI_HP_IS_IFS_BIT)
#define AHCI_HP_IS_INFS         (1U<<AHCI_HP_IS_INFS_BIT)
#define AHCI_HP_IS_OFS          (1U<<AHCI_HP_IS_OFS_BIT)
#define AHCI_HP_IS_IPMS         (1U<<AHCI_HP_IS_IPMS_BIT)
#define AHCI_HP_IS_PRCS         (1U<<AHCI_HP_IS_PRCS_BIT)
#define AHCI_HP_IS_DMPS         (1U<<AHCI_HP_IS_DMPS_BIT)
#define AHCI_HP_IS_PCS          (1U<<AHCI_HP_IS_PCS_BIT)
#define AHCI_HP_IS_DPS          (1U<<AHCI_HP_IS_DPS_BIT)
#define AHCI_HP_IS_UFS          (1U<<AHCI_HP_IS_UFS_BIT)
#define AHCI_HP_IS_SDBS         (1U<<AHCI_HP_IS_SDBS_BIT)
#define AHCI_HP_IS_DSS          (1U<<AHCI_HP_IS_DSS_BIT)
#define AHCI_HP_IS_PSS          (1U<<AHCI_HP_IS_PSS_BIT)
#define AHCI_HP_IS_DHRS         (1U<<AHCI_HP_IS_DHRS_BIT)

//
// hba_port_t::intr_en

// Cold Port Detect interrupt enable
#define AHCI_HP_IE_CPDS_BIT     31

// Task File Error interrupt enable
#define AHCI_HP_IE_TFES_BIT     30

// Host Bus Fatal Error interrupt enable
#define AHCI_HP_IE_HBFS_BIT     29

// Host Bus Data Error interrupt enable
#define AHCI_HP_IE_HBDS_BIT     28

// Interface Fatal Error interrupt enable
#define AHCI_HP_IE_IFS_BIT      27

// Interface Non-fatal Error interrupt enable
#define AHCI_HP_IE_INFS_BIT     26

// Overflow interrupt enable
#define AHCI_HP_IE_OFS_BIT      24

// Incorrect Port Multiplier interrupt enable
#define AHCI_HP_IE_IPMS_BIT     23

// PhyRdy Change interrupt enable
#define AHCI_HP_IE_PRCS_BIT     22

// Device Mechanical Presence interrupt enable
#define AHCI_HP_IE_DMPS_BIT     7

// Port Connect Change interrupt enable
#define AHCI_HP_IE_PCS_BIT      6

// Descriptor Processed interrupt enable
#define AHCI_HP_IE_DPS_BIT      5

// Unknown FIS interrupt enable
#define AHCI_HP_IE_UFS_BIT      4

// Set Device Bits interrupt enable
#define AHCI_HP_IE_SDBS_BIT     3

// DMA Setup FIS interrupt enable
#define AHCI_HP_IE_DSS_BIT      2

// PIO Setup FIS interrupt enable
#define AHCI_HP_IE_PSS_BIT      1

// Device to Host Register FIS interrupt enable
#define AHCI_HP_IE_DHRS_BIT     0

#define AHCI_HP_IE_CPDS         (1U<<AHCI_HP_IE_CPDS_BIT)
#define AHCI_HP_IE_TFES         (1U<<AHCI_HP_IE_TFES_BIT)
#define AHCI_HP_IE_HBFS         (1U<<AHCI_HP_IE_HBFS_BIT)
#define AHCI_HP_IE_HBDS         (1U<<AHCI_HP_IE_HBDS_BIT)
#define AHCI_HP_IE_IFS          (1U<<AHCI_HP_IE_IFS_BIT)
#define AHCI_HP_IE_INFS         (1U<<AHCI_HP_IE_INFS_BIT)
#define AHCI_HP_IE_OFS          (1U<<AHCI_HP_IE_OFS_BIT)
#define AHCI_HP_IE_IPMS         (1U<<AHCI_HP_IE_IPMS_BIT)
#define AHCI_HP_IE_PRCS         (1U<<AHCI_HP_IE_PRCS_BIT)
#define AHCI_HP_IE_DMPS         (1U<<AHCI_HP_IE_DMPS_BIT)
#define AHCI_HP_IE_PCS          (1U<<AHCI_HP_IE_PCS_BIT)
#define AHCI_HP_IE_DPS          (1U<<AHCI_HP_IE_DPS_BIT)
#define AHCI_HP_IE_UFS          (1U<<AHCI_HP_IE_UFS_BIT)
#define AHCI_HP_IE_SDBS         (1U<<AHCI_HP_IE_SDBS_BIT)
#define AHCI_HP_IE_DSS          (1U<<AHCI_HP_IE_DSS_BIT)
#define AHCI_HP_IE_PSS          (1U<<AHCI_HP_IE_PSS_BIT)
#define AHCI_HP_IE_DHRS         (1U<<AHCI_HP_IE_DHRS_BIT)

//
// hba_port_t::cmd

// Interface Communication Control
#define AHCI_HP_CMD_ICC_BIT     28

// Aggressive Slumber / Partial
#define AHCI_HP_CMD_ASP_BIT     27

// Aggressive Link Power Manager Enable
#define AHCI_HP_CMD_ALPE_BIT    26

// Drive LED on ATAPI Enablr
#define AHCI_HP_CMD_DLAE_BIT    25

// Device is ATAPI
#define AHCI_HP_CMD_ATAPI_BIT   24

// Automatic Partial to Slumber Transitions Enabled
#define AHCI_HP_CMD_APSTE_BIT   23

// FIS Based Switching Capable Port
#define AHCI_HP_CMD_FBSCP_BIT   22

// External SATA Port
#define AHCI_HP_CMD_ESP_BIT     21

// Cold Presence Detect
#define AHCI_HP_CMD_CPD_BIT     20

// Mechanical Presence Switch attached to Port
#define AHCI_HP_CMD_MPSP_BIT    19

// Hot Plug Capable Port
#define AHCI_HP_CMD_HPCP_BIT    18

// Port Multiplier Attached
#define AHCI_HP_CMD_PMA_BIT     17

// Cold Presence State
#define AHCI_HP_CMD_CPS_BIT     16

// Command List Running
#define AHCI_HP_CMD_CR_BIT      15

// FIS Receive Running
#define AHCI_HP_CMD_FR_BIT      14

// Mechanical Presense Switch State
#define AHCI_HP_CMD_MPSS_BIT    13

// Current Command Slot
#define AHCI_HP_CMD_CCS_BIT     8

// FIS Receive Enable
#define AHCI_HP_CMD_FRE_BIT     4

// Command List Override
#define AHCI_HP_CMD_CLO_BIT     3

// Power On Device
#define AHCI_HP_CMD_POD_BIT     2

// Spin Up Device
#define AHCI_HP_CMD_SUD_BIT     1

// Start
#define AHCI_HP_CMD_ST_BIT      0

#define AHCI_HP_CMD_ICC         (1U<<AHCI_HP_CMD_ICC_BIT)
#define AHCI_HP_CMD_ASP         (1U<<AHCI_HP_CMD_ASP_BIT)
#define AHCI_HP_CMD_ALPE        (1U<<AHCI_HP_CMD_ALPE_BIT)
#define AHCI_HP_CMD_DLAE        (1U<<AHCI_HP_CMD_DLAE_BIT)
#define AHCI_HP_CMD_ATAPI       (1U<<AHCI_HP_CMD_ATAPI_BIT)
#define AHCI_HP_CMD_APSTE       (1U<<AHCI_HP_CMD_APSTE_BIT)
#define AHCI_HP_CMD_FBSCP       (1U<<AHCI_HP_CMD_FBSCP_BIT)
#define AHCI_HP_CMD_ESP         (1U<<AHCI_HP_CMD_ESP_BIT)
#define AHCI_HP_CMD_CPD         (1U<<AHCI_HP_CMD_CPD_BIT)
#define AHCI_HP_CMD_MPSP        (1U<<AHCI_HP_CMD_MPSP_BIT)
#define AHCI_HP_CMD_HPCP        (1U<<AHCI_HP_CMD_HPCP_BIT)
#define AHCI_HP_CMD_PMA         (1U<<AHCI_HP_CMD_PMA_BIT)
#define AHCI_HP_CMD_CPS         (1U<<AHCI_HP_CMD_CPS_BIT)
#define AHCI_HP_CMD_CR          (1U<<AHCI_HP_CMD_CR_BIT)
#define AHCI_HP_CMD_FR          (1U<<AHCI_HP_CMD_FR_BIT)
#define AHCI_HP_CMD_MPSS        (1U<<AHCI_HP_CMD_MPSS_BIT)
#define AHCI_HP_CMD_CCS         (1U<<AHCI_HP_CMD_CCS_BIT)
#define AHCI_HP_CMD_FRE         (1U<<AHCI_HP_CMD_FRE_BIT)
#define AHCI_HP_CMD_CLO         (1U<<AHCI_HP_CMD_CLO_BIT)
#define AHCI_HP_CMD_POD         (1U<<AHCI_HP_CMD_POD_BIT)
#define AHCI_HP_CMD_SUD         (1U<<AHCI_HP_CMD_SUD_BIT)
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

// 4.2.1
typedef struct hba_fis_t {
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
} hba_fis_t;

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
typedef struct hba_cmd_hdr_t {
    // Header
    uint16_t hdr;

    // Physical Region Descriptor Table Length (entries)
    uint16_t prdtl;

    // PRD Byte Count
    uint32_t prdbc;

    // Command Table Base Address (128 byte aligned)
    uint64_t ctba;

    uint32_t rsv[4];
} hba_cmd_hdr_t;

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
typedef struct hba_prdt_ent_t {
    // Data Base Address
    uint64_t dba;
    uint32_t rsv;
    // Data Byte Count
    uint32_t dbc_intr;
} hba_prdt_ent_t;

// Makes command table entry 1KB
#define AHCI_CMD_TBL_ENT_MAX_PRD 56

// 4.2.3 1KB command
typedef struct hba_cmd_tbl_ent_t {
    union {
        ahci_fis_d2h_t d2h;
        ahci_fis_h2d_t h2d;
        char filler[64];
    } cfis;
    char atapi_fis[16];
    char filler[0x80-0x50];

    // At offset 0x80
    hba_prdt_ent_t prdts[AHCI_CMD_TBL_ENT_MAX_PRD];
} hba_cmd_tbl_ent_t;

C_ASSERT(sizeof(hba_cmd_tbl_ent_t) == 1024);

typedef struct async_callback_t {
    void (*callback)(int error, uintptr_t arg);
    uintptr_t callback_arg;
} async_callback_t;

typedef struct hba_port_info_t {
    hba_fis_t volatile *fis;
    hba_cmd_hdr_t volatile *cmd_hdr;
    hba_cmd_tbl_ent_t volatile *cmd_tbl;
    async_callback_t callbacks[32];
    uint32_t sector_size;
    uint32_t volatile cmd_issued;

    // Keep track of slot order
    spinlock_t lock;
    uint8_t issue_queue[32];
    uint8_t issue_head;
    uint8_t issue_tail;
} hba_port_info_t;

#define AHCI_PE_INTR_BIT    31
#define AHCI_PE_DBC_BIT     1
#define AHCI_PE_DBC_n(n)    ((n) | 1)

// Drive interface
struct ahci_if_t {
    storage_if_vtbl_t *vtbl;

    pci_config_hdr_t config;

    // Linear addresses
    hba_host_ctl_t volatile *mmio_base;

    void *buffers;

    hba_port_info_t port_info[32];

    int irq;
    int ports_impl;
    int num_cmd_slots;
};

// Drive
struct ahci_dev_t {
    storage_dev_vtbl_t *vtbl;
    ahci_if_t *if_;
    int port;
};

#define AHCI_MAX_DEVICES    16
static ahci_if_t ahci_devices[AHCI_MAX_DEVICES];
static unsigned ahci_count;

#define AHCI_MAX_DRIVES    64
static ahci_dev_t ahci_drives[AHCI_MAX_DRIVES];
static unsigned ahci_drive_count;

static int ahci_supports_64bit(ahci_if_t *dev)
{
    return (dev->mmio_base->cap & AHCI_HC_CAP_S64A) != 0;
}

// Must be holding port spinlock
static void ahci_release_slot(ahci_if_t *dev, int port_num, int slot)
{
    assert(slot >= 0);

    hba_port_info_t *pi = dev->port_info + port_num;

    // Make sure it was really acquired
    assert(pi->cmd_issued & (1U<<slot));

    // Make sure this was really the oldest command
    assert(pi->issue_queue[pi->issue_tail] == slot);
    // Advance queue tail
    pi->issue_tail = (pi->issue_tail + 1) & 31;

    // Mark slot as not in use
    atomic_and_uint32(&pi->cmd_issued, ~(1U<<slot));
}

// Acquire slot that is not in use
// Returns -1 if all slots are in use
// Must be holding port spinlock
static int ahci_acquire_slot(ahci_if_t *dev, int port_num)
{
    hba_port_info_t *pi = dev->port_info + port_num;
    //hba_port_t volatile *port = dev->mmio_base->ports + port_num;

    for (;;) {
        // Build bitmask of slots in use
        uint32_t busy_mask = pi->cmd_issued;

        // If every slot is busy
        if (busy_mask == 0xFFFFFFFF ||
                (dev->num_cmd_slots < 32 &&
                busy_mask == ((1U<<dev->num_cmd_slots)-1)))
            return -1;

        // Find first zero
        int slot = bit_lsb_set_32(~busy_mask);

        uint32_t old_busy = pi->cmd_issued;
        if ((old_busy & (1U<<slot)) == 0) {
            if (old_busy == atomic_cmpxchg(
                        &pi->cmd_issued, old_busy,
                        old_busy | (1U<<slot))) {
                // Write slot number to issue queue
                pi->issue_queue[pi->issue_head] = slot;
                pi->issue_head = (pi->issue_head + 1) & 31;

                return slot;
            }
        }
    }
}

static void ahci_handle_port_irqs(ahci_if_t *dev, int port_num)
{
    hba_host_ctl_t volatile *hba = dev->mmio_base;
    hba_port_t volatile *port = hba->ports + port_num;
    hba_port_info_t volatile *pi = dev->port_info + port_num;

    spinlock_hold_t hold = spinlock_lock_noirq(&pi->lock);

    // Read command slot interrupt status
    int slot = pi->issue_queue[pi->issue_tail];

    int error = 0;

    if (port->sata_err != 0)
        error = 1;

    if ((port->taskfile_data & AHCI_HP_TFD_ERR) ||
            (port->taskfile_data & AHCI_HP_TFD_SERR_BIT))
        error = 2;

    // Invoke completion callback
    async_callback_t *callback = dev->port_info[port_num].callbacks + slot;
    if (callback->callback) {
        callback->callback(error, callback->callback_arg);
        callback->callback = 0;
        callback->callback_arg = 0;
    }


    // Acknowledge slot interrupt
    port->intr_status |= port->intr_status;

    ahci_release_slot(dev, port_num, slot);

    // Mark slots completed
    //atomic_and_uint32(&pi->cmd_issued, ~(1U<<slot));

    spinlock_unlock_noirq(&pi->lock, &hold);
}

static void *ahci_irq_handler(int irq, void *ctx)
{
    for (unsigned i = 0; i < ahci_count; ++i) {
        ahci_if_t *dev = ahci_devices + i;

        if (dev->irq != irq)
            continue;

        // Call callback on every port that has an interrupt pending
        int port;
        for (uint32_t intr_status = dev->mmio_base->intr_status;
             intr_status != 0; intr_status &= ~(1U<<port)) {
            // Look through each port
            port = bit_lsb_set_32(intr_status);

            ahci_handle_port_irqs(dev, port);

            // Acknowledge the interrupt on the port
            dev->mmio_base->intr_status = (1U<<port);
        }
    }

    return ctx;
}

static void ahci_port_stop(ahci_if_t *dev, int port_num)
{
    hba_port_t volatile *port = dev->mmio_base->ports + port_num;

    // Clear start bit
    // Clear FIS receive enable bit
    port->cmd &= ~(AHCI_HP_CMD_ST | AHCI_HP_CMD_FRE);

    atomic_barrier();

    // Wait until there is not a command running,
    // and there is not a FIS receive running
    while (port->cmd & (AHCI_HP_CMD_CR | AHCI_HP_CMD_FR))
        thread_yield();
}

static void ahci_port_start(ahci_if_t *dev, int port_num)
{
    hba_port_t volatile *port = dev->mmio_base->ports + port_num;

    // Wait until there is not a command running, and
    // there is not a FIS receive running
    while (port->cmd & (AHCI_HP_CMD_CR | AHCI_HP_CMD_FR))
        thread_yield();

    // Set start and FIS receive enable bit
    port->cmd |= AHCI_HP_CMD_ST | AHCI_HP_CMD_FRE;
}

// Stop each implemented port
static void ahci_port_stop_all(ahci_if_t *dev)
{
    int port;
    for (uint32_t impl = dev->ports_impl;
         impl != 0; impl &= ~(1U<<port)) {
        port = bit_lsb_set_32(impl);
        ahci_port_stop(dev, port);
    }
}

// Start each implemented port
static void ahci_port_start_all(ahci_if_t *dev)
{
    int port;
    for (uint32_t impl = dev->ports_impl;
         impl != 0; impl &= ~(1U<<port)) {
        port = bit_lsb_set_32(impl);
        ahci_port_start(dev, port);
    }
}

static void ahci_rw(ahci_if_t *dev, int port_num, uint64_t lba,
                      void *data, uint32_t count, int is_read,
                      void (*callback)(int error, uintptr_t arg),
                      uintptr_t callback_arg)
{
    hba_port_info_t *pi = dev->port_info + port_num;
    hba_port_t volatile *port = dev->mmio_base->ports + port_num;

    mmphysrange_t ranges[AHCI_CMD_TBL_ENT_MAX_PRD];
    size_t ranges_count;

    spinlock_hold_t hold = spinlock_lock_noirq(&pi->lock);

    int slot;
    for (;;) {
        slot = ahci_acquire_slot(dev, port_num);
        if (slot >= 0)
            break;

        spinlock_unlock_noirq(&pi->lock, &hold);
        thread_yield();
        hold = spinlock_lock_noirq(&pi->lock);
    }

    hba_cmd_hdr_t volatile *cmd_hdr = pi->cmd_hdr + slot;
    hba_cmd_tbl_ent_t volatile *cmd_tbl_ent = pi->cmd_tbl + slot;

    ranges_count = mphysranges(ranges, countof(ranges),
                               data, count * pi->sector_size,
                               2<<20);

    for (size_t i = 0; i < ranges_count; ++i) {
        cmd_tbl_ent->prdts[i].dba = ranges[i].physaddr;
        cmd_tbl_ent->prdts[i].dbc_intr = AHCI_PE_DBC_n(ranges[i].size);
    }
    atomic_barrier();

    cmd_tbl_ent->cfis.h2d.fis_type = FIS_TYPE_REG_H2D;
    cmd_tbl_ent->cfis.h2d.ctl = AHCI_FIS_CTL_CMD;
    cmd_tbl_ent->cfis.h2d.command = is_read
            ? ATA_CMD_READ_DMA_EXT
            : ATA_CMD_WRITE_DMA_EXT;
    cmd_tbl_ent->cfis.h2d.lba0 = lba & 0xFFFF;
    cmd_tbl_ent->cfis.h2d.lba2 = (lba >> 16) & 0xFF;
    cmd_tbl_ent->cfis.h2d.lba3 = (lba >> 24) & 0xFFFF;
    cmd_tbl_ent->cfis.h2d.lba5 = (lba >> 40);
    cmd_tbl_ent->cfis.h2d.count = count;

    // LBA
    cmd_tbl_ent->cfis.d2h.device = 1U<<6;
    atomic_barrier();

    cmd_hdr->hdr = AHCI_CH_LEN_n(sizeof(pi->cmd_tbl->cfis.h2d) >> 2);
    cmd_hdr->prdbc = 0;
    cmd_hdr->prdtl = ranges_count;

    pi->callbacks[slot].callback = callback;
    pi->callbacks[slot].callback_arg = callback_arg;

    atomic_barrier();
    port->cmd_issue = (1U<<slot);

    spinlock_unlock_noirq(&pi->lock, &hold);
}

// The command engine must be stopped before calling ahci_perform_detect
static void ahci_perform_detect(ahci_if_t *dev, int port_num)
{
    hba_port_t volatile *port = dev->mmio_base->ports + port_num;

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

static void ahci_rebase(ahci_if_t *dev)
{
    // Stop all ports
    ahci_port_stop_all(dev);

    int addr_type = ahci_supports_64bit(dev)
            ? 0 //MAP_WRITETHRU | MAP_NOCACHE
            : MAP_32BIT //| MAP_WRITETHRU | MAP_NOCACHE
            ;

    // Loop through the implemented ports
    uint32_t ports_impl = dev->ports_impl;
    uint32_t slots_impl = dev->num_cmd_slots;

    // Initial "slot busy" mask marks unimplemented slots
    // as permanently busy
    uint32_t init_busy_mask = (slots_impl == 32)
            ? 0
            : ((uint32_t)~0<<slots_impl);

    for (int port_num = 0; port_num < 32; ++port_num) {
        if (!(ports_impl & (1U<<port_num)))
            continue;

        printk("Initializing AHCI device port %d\n", port_num);

        hba_port_t volatile *port = dev->mmio_base->ports + port_num;
        hba_port_info_t volatile *pi = dev->port_info + port_num;

        ahci_perform_detect(dev, port_num);

        // debug hack
        if (port->sig != SATA_SIG_ATA)
            continue;

        size_t port_buffer_size = 0;

        // 256 byte FIS buffer
        port_buffer_size += sizeof(hba_fis_t);
        // One cmd hdr per slot
        port_buffer_size += sizeof(hba_cmd_hdr_t) * 32;
        // One cmd tbl per slot
        port_buffer_size += sizeof(hba_cmd_tbl_ent_t) * 32;

        void *buffers = mmap(
                    0, port_buffer_size, PROT_READ | PROT_WRITE,
                    addr_type, -1, 0);
        memset(buffers, 0, port_buffer_size);

        hba_cmd_hdr_t volatile *cmd_hdr = buffers;
        hba_cmd_tbl_ent_t volatile *cmd_tbl = (void*)(cmd_hdr + 32);
        hba_fis_t volatile *fis = (void*)(cmd_tbl + 32);

        // Store linear addresses for writing to buffers
        pi->fis = fis;
        pi->cmd_hdr = cmd_hdr;
        pi->cmd_tbl = cmd_tbl;

        assert(port->sata_act == 0);

        // Initialize with all unimplemented slots busy
        // (Workaround initially active slot on QEMU)
        pi->cmd_issued = init_busy_mask;

        // Hack
        dev->port_info[port_num].sector_size = 512;

        atomic_barrier();
        port->cmd_list_base = mphysaddr((void*)cmd_hdr);
        port->fis_base = mphysaddr((void*)fis);
        atomic_barrier();

        // Set command table base addresses (physical)
        for (uint32_t slot = 0; slot < slots_impl; ++slot)
            cmd_hdr[slot].ctba = mphysaddr((void*)(cmd_tbl + slot));

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

    ahci_port_start_all(dev);

    // Enable interrupts overall
    dev->mmio_base->host_ctl |= AHCI_HC_HC_IE;
}

static void ahci_bios_handoff(ahci_if_t *dev)
{
    if ((dev->mmio_base->cap2 & AHCI_HC_CAP2_BOH) == 0)
        return;

    // Request BIOS handoff
    dev->mmio_base->bios_handoff =
            (dev->mmio_base->bios_handoff & ~AHCI_HC_BOH_OOC) |
            AHCI_HC_BOH_OOS;

    atomic_barrier();
    while ((dev->mmio_base->bios_handoff &
            (AHCI_HC_BOH_BOS | AHCI_HC_BOH_OOS)) !=
           AHCI_HC_BOH_OOS)
        thread_yield();
}

static if_list_t ahci_if_detect(void)
{
    if_list_t list = {ahci_devices, sizeof(*ahci_devices), 0};
    pci_dev_iterator_t pci_iter;

    if (!pci_enumerate_begin(&pci_iter, 1, 6))
        return list;

    do {
        assert(pci_iter.dev_class == 1);
        assert(pci_iter.subclass == 6);

        // Ignore controllers with AHCI base address not set
        if (pci_iter.config.base_addr[5] == 0)
            continue;

        printk("Found AHCI Device BAR ht=%x %u/%u/%u d=%x s=%x: ",
               pci_iter.config.header_type,
               pci_iter.bus, pci_iter.slot, pci_iter.func,
               pci_iter.dev_class, pci_iter.subclass);

        for (int i = 0; i < 6; ++i) {
            printk("%x ",
                   pci_iter.config.base_addr[i]);
        }

        printk("\nIRQ line=%d, IRQ pin=%d\n",
               pci_iter.config.irq_line,
               pci_iter.config.irq_pin);

        if (ahci_count < countof(ahci_devices)) {
            ahci_if_t *dev = ahci_devices + ahci_count++;

            dev->vtbl = &ahci_if_device_vtbl;

            dev->config = pci_iter.config;

            dev->mmio_base =
                    mmap((void*)(uintptr_t)pci_iter.config.base_addr[5],
                    0x1100, PROT_READ | PROT_WRITE,
                    MAP_PHYSICAL, -1, 0);

            ahci_bios_handoff(dev);

            // Cache implemented port bitmask
            dev->ports_impl = dev->mmio_base->ports_impl;

            // Cache number of command slots per port
            dev->num_cmd_slots = 1 + ((dev->mmio_base->cap >>
                                  AHCI_HC_CAP_NCS_BIT) &
                                  AHCI_HC_CAP_NCS_MASK);

            dev->irq = pci_iter.config.irq_line;

            ahci_rebase(dev);

            irq_hook(dev->irq, ahci_irq_handler);
            irq_setmask(dev->irq, 1);
        }
    } while (pci_enumerate_next(&pci_iter));

    list.count = ahci_count;

    return list;
}

//
// device registration

static if_list_t ahci_if_detect_devices(storage_if_base_t *if_)
{
    if_list_t list = {ahci_drives,sizeof(*ahci_drives),0};
    STORAGE_IF_DEV_PTR(if_);

    for (int port_num = 0; port_num < 32; ++port_num) {
        if (!(self->ports_impl & (1U<<port_num)))
            continue;

        hba_port_t volatile *port = self->mmio_base->ports + port_num;

        if (port->sig == SATA_SIG_ATA) {
            ahci_dev_t *drive = ahci_drives + ahci_drive_count++;
            drive->vtbl = &ahci_dev_device_vtbl;
            drive->if_ = self;
            drive->port = port_num;
        }
    }

    list.count = ahci_drive_count;

    return list;
}

static void ahci_if_cleanup(storage_if_base_t *dev)
{
    (void)dev;
}

static void ahci_dev_cleanup(storage_dev_base_t *dev)
{
    STORAGE_DEV_DEV_PTR_UNUSED(dev);
}

typedef struct ahci_blocking_io_t {
    mutex_t lock;
    condition_var_t done_cond;

    int done;
    int err;
    uint64_t lba;
} ahci_blocking_io_t;

static void ahci_async_complete(int error, uintptr_t arg)
{
    ahci_blocking_io_t *state = (void*)arg;

    mutex_lock_noyield(&state->lock);
    state->err = error;
    state->done = 1;
    mutex_unlock(&state->lock);
    condvar_wake_one(&state->done_cond);
}

static int ahci_dev_io(storage_dev_base_t *dev,
                       void *data, uint64_t count,
                       uint64_t lba, int is_read)
{
    STORAGE_DEV_DEV_PTR(dev);

    ahci_blocking_io_t block_state;
    memset(&block_state, 0, sizeof(block_state));

    mutex_init(&block_state.lock);
    condvar_init(&block_state.done_cond);

    int intr_were_enabled = cpu_irq_disable();

    mutex_lock(&block_state.lock);

    block_state.lba = lba;

    ahci_rw(self->if_, self->port, lba, data, count, is_read,
            ahci_async_complete, (uintptr_t)&block_state);

    while (!block_state.done)
        condvar_wait(&block_state.done_cond, &block_state.lock);

    mutex_unlock(&block_state.lock);

    condvar_destroy(&block_state.done_cond);
    mutex_destroy(&block_state.lock);

    cpu_irq_toggle(intr_were_enabled);

    return block_state.err;
}

static int ahci_dev_read(storage_dev_base_t *dev,
                         void *data, uint64_t count,
                         uint64_t lba)
{
    return ahci_dev_io(dev, data, count, lba, 1);
}

static int ahci_dev_write(storage_dev_base_t *dev,
                void *data, uint64_t count, uint64_t lba)
{
    return ahci_dev_io(dev, data, count, lba, 0);
}

static int ahci_dev_flush(storage_dev_base_t *dev)
{
    STORAGE_DEV_DEV_PTR_UNUSED(dev);
    return 0;
}

REGISTER_storage_if_DEVICE(ahci);
DEFINE_storage_dev_DEVICE(ahci);
