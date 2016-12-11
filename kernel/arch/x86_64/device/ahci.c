#include "device/ahci.h"
#include "device/pci.h"
#include "printk.h"
#include "mm.h"
#include "assert.h"

#if 0

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

#define AHCI_FIS_CTL_PORTMUX_BIT    4
#define AHCI_FIS_CTL_PORTMUX_BITS   4
#define AHCI_FIS_CTL_CMD_BIT        0

#define AHCI_FIS_CTL_PORTMUX_MASK   ((1<<AHCI_FIS_CTL_PORTMUX_BITS)-1)
#define AHCI_FIS_CTL_PORTMUX_n(n)   ((n)<<AHCI_FIS_CTL_PORTMUX_BITS)

#define AHCI_FIS_CTL_CMD            (1<<AHCI_FIS_CTL_CMD_BIT)
#define AHCI_FIS_CTL_CTL            (0<<AHCI_FIS_CTL_CMD_BIT)

typedef struct ahci_fis_d2h_t
{
    // FIS_TYPE_REG_D2H
    uint8_t fis_type;

    // PORTMUX and INTR
    uint8_t ctl;

    // Status
    uint8_t status;

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

    uint32_t rsv4;    // Reserved
} ahci_fis_d2h_t;

#define AHCI_FIS_CTL_INTR_BIT   1
#define AHCI_FIS_CTL_INTR       (1<<AHCI_FIS_CTL_INTR_BIT)

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

typedef struct ahci_dma_setup_t
{
    // FIS_TYPE_DMA_SETUP
    uint8_t	fis_type;

    // PORTMUX, DIR, INTR, AUTO
    uint8_t	ctl;

    // Reserved
    uint16_t rsv;

    // DMA Buffer Identifier. Used to Identify DMA buffer in host memory.
    // SATA Spec says host specific and not in Spec.
    // Trying AHCI spec might work.
    uint64_t dma_buf_id;

    // Reserved
    uint32_t rsv2;

    // Byte offset into buffer. First 2 bits must be 0
    uint32_t dma_buf_ofs;

    //Number of bytes to transfer. Bit 0 must be 0
    uint32_t transfer_count;

    //Reserved
    uint32_t rsv3;
} ahci_dma_setup_t;

#define AHCI_FIS_CTL_DIR_BIT    2
#define AHCI_FIS_CTL_AUTO_BIT   0

#define AHCI_FIS_CTL_DIR        (1<<AHCI_FIS_CTL_DIR_BIT)
#define AHCI_FIS_AUTO_DIR       (1<<AHCI_FIS_CTL_AUTO_BIT)

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

    // SATA active (SCR3:SActive)
    uint32_t sata_act;

    // command issue
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

#define AHCI_HC_CAP_S64A        (1<<AHCI_HC_CAP_S64A_BIT)
#define AHCI_HC_CAP_SNCQ        (1<<AHCI_HC_CAP_SNCQ_BIT)
#define AHCI_HC_CAP_SSNTF       (1<<AHCI_HC_CAP_SSNTF_BIT)
#define AHCI_HC_CAP_SMPS        (1<<AHCI_HC_CAP_SMPS_BIT)
#define AHCI_HC_CAP_SSS         (1<<AHCI_HC_CAP_SSS_BIT)
#define AHCI_HC_CAP_SALP        (1<<AHCI_HC_CAP_SALP_BIT)
#define AHCI_HC_CAP_SAL         (1<<AHCI_HC_CAP_SAL_BIT)
#define AHCI_HC_CAP_SCLO        (1<<AHCI_HC_CAP_SCLO_BIT)
#define AHCI_HC_CAP_ISS         (1<<AHCI_HC_CAP_ISS_BIT)
#define AHCI_HC_CAP_SAM         (1<<AHCI_HC_CAP_SAM_BIT)
#define AHCI_HC_CAP_SPM         (1<<AHCI_HC_CAP_SPM_BIT)
#define AHCI_HC_CAP_FBSS        (1<<AHCI_HC_CAP_FBSS_BIT)
#define AHCI_HC_CAP_PMD         (1<<AHCI_HC_CAP_PMD_BIT)
#define AHCI_HC_CAP_SSC         (1<<AHCI_HC_CAP_SSC_BIT)
#define AHCI_HC_CAP_PSC         (1<<AHCI_HC_CAP_PSC_BIT)
#define AHCI_HC_CAP_NCS         (1<<AHCI_HC_CAP_NCS_BIT)
#define AHCI_HC_CAP_CCCS        (1<<AHCI_HC_CAP_CCCS_BIT)
#define AHCI_HC_CAP_EMS         (1<<AHCI_HC_CAP_EMS_BIT)
#define AHCI_HC_CAP_SXS         (1<<AHCI_HC_CAP_SXS_BIT)
#define AHCI_HC_CAP_NP          (1<<AHCI_HC_CAP_NP_BIT)

#define AHCI_HC_CAP_ISS_MASK    ((1<<AHCI_HC_CAP_ISS_BITS)-1)
#define AHCI_HC_CAP_NCS_MASK    ((1<<AHCI_HC_CAP_NCS_BITS)-1)
#define AHCI_HC_CAP_NP_MASK     ((1<<AHCI_HC_CAP_NP_BITS)-1)

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

#define AHCI_HC_HC_AE           (1<<AHCI_HC_HC_AE_BIT)
#define AHCI_HC_HC_MRSM         (1<<AHCI_HC_HC_MRSM_BIT)
#define AHCI_HC_HC_IE           (1<<AHCI_HC_HC_IE_BIT)
#define AHCI_HC_HC_HR           (1<<AHCI_HC_HC_HR_BIT)

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

#define AHCI_HP_IS_CPDS         (1<<AHCI_HP_IS_CPDS_BIT)
#define AHCI_HP_IS_TFES         (1<<AHCI_HP_IS_TFES_BIT)
#define AHCI_HP_IS_HBFS         (1<<AHCI_HP_IS_HBFS_BIT)
#define AHCI_HP_IS_HBDS         (1<<AHCI_HP_IS_HBDS_BIT)
#define AHCI_HP_IS_IFS          (1<<AHCI_HP_IS_IFS_BIT)
#define AHCI_HP_IS_INFS         (1<<AHCI_HP_IS_INFS_BIT)
#define AHCI_HP_IS_OFS          (1<<AHCI_HP_IS_OFS_BIT)
#define AHCI_HP_IS_IPMS         (1<<AHCI_HP_IS_IPMS_BIT)
#define AHCI_HP_IS_PRCS         (1<<AHCI_HP_IS_PRCS_BIT)
#define AHCI_HP_IS_DMPS         (1<<AHCI_HP_IS_DMPS_BIT)
#define AHCI_HP_IS_PCS          (1<<AHCI_HP_IS_PCS_BIT)
#define AHCI_HP_IS_DPS          (1<<AHCI_HP_IS_DPS_BIT)
#define AHCI_HP_IS_UFS          (1<<AHCI_HP_IS_UFS_BIT)
#define AHCI_HP_IS_SDBS         (1<<AHCI_HP_IS_SDBS_BIT)
#define AHCI_HP_IS_DSS          (1<<AHCI_HP_IS_DSS_BIT)
#define AHCI_HP_IS_PSS          (1<<AHCI_HP_IS_PSS_BIT)
#define AHCI_HP_IS_DHRS         (1<<AHCI_HP_IS_DHRS_BIT)

// 0000

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

#define AHCI_HP_IE_CPDS         (1<<AHCI_HP_IE_CPDS_BIT)
#define AHCI_HP_IE_TFES         (1<<AHCI_HP_IE_TFES_BIT)
#define AHCI_HP_IE_HBFS         (1<<AHCI_HP_IE_HBFS_BIT)
#define AHCI_HP_IE_HBDS         (1<<AHCI_HP_IE_HBDS_BIT)
#define AHCI_HP_IE_IFS          (1<<AHCI_HP_IE_IFS_BIT)
#define AHCI_HP_IE_INFS         (1<<AHCI_HP_IE_INFS_BIT)
#define AHCI_HP_IE_OFS          (1<<AHCI_HP_IE_OFS_BIT)
#define AHCI_HP_IE_IPMS         (1<<AHCI_HP_IE_IPMS_BIT)
#define AHCI_HP_IE_PRCS         (1<<AHCI_HP_IE_PRCS_BIT)
#define AHCI_HP_IE_DMPS         (1<<AHCI_HP_IE_DMPS_BIT)
#define AHCI_HP_IE_PCS          (1<<AHCI_HP_IE_PCS_BIT)
#define AHCI_HP_IE_DPS          (1<<AHCI_HP_IE_DPS_BIT)
#define AHCI_HP_IE_UFS          (1<<AHCI_HP_IE_UFS_BIT)
#define AHCI_HP_IE_SDBS         (1<<AHCI_HP_IE_SDBS_BIT)
#define AHCI_HP_IE_DSS          (1<<AHCI_HP_IE_DSS_BIT)
#define AHCI_HP_IE_PSS          (1<<AHCI_HP_IE_PSS_BIT)
#define AHCI_HP_IE_DHRS         (1<<AHCI_HP_IE_DHRS_BIT)

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

#define AHCI_HP_CMD_ICC         (1<<AHCI_HP_CMD_ICC_BIT)
#define AHCI_HP_CMD_ASP         (1<<AHCI_HP_CMD_ASP_BIT)
#define AHCI_HP_CMD_ALPE        (1<<AHCI_HP_CMD_ALPE_BIT)
#define AHCI_HP_CMD_DLAE        (1<<AHCI_HP_CMD_DLAE_BIT)
#define AHCI_HP_CMD_ATAPI       (1<<AHCI_HP_CMD_ATAPI_BIT)
#define AHCI_HP_CMD_APSTE       (1<<AHCI_HP_CMD_APSTE_BIT)
#define AHCI_HP_CMD_FBSCP       (1<<AHCI_HP_CMD_FBSCP_BIT)
#define AHCI_HP_CMD_ESP         (1<<AHCI_HP_CMD_ESP_BIT)
#define AHCI_HP_CMD_CPD         (1<<AHCI_HP_CMD_CPD_BIT)
#define AHCI_HP_CMD_MPSP        (1<<AHCI_HP_CMD_MPSP_BIT)
#define AHCI_HP_CMD_HPCP        (1<<AHCI_HP_CMD_HPCP_BIT)
#define AHCI_HP_CMD_PMA         (1<<AHCI_HP_CMD_PMA_BIT)
#define AHCI_HP_CMD_CPS         (1<<AHCI_HP_CMD_CPS_BIT)
#define AHCI_HP_CMD_CR          (1<<AHCI_HP_CMD_CR_BIT)
#define AHCI_HP_CMD_FR          (1<<AHCI_HP_CMD_FR_BIT)
#define AHCI_HP_CMD_MPSS        (1<<AHCI_HP_CMD_MPSS_BIT)
#define AHCI_HP_CMD_CCS         (1<<AHCI_HP_CMD_CCS_BIT)
#define AHCI_HP_CMD_FRE         (1<<AHCI_HP_CMD_FRE_BIT)
#define AHCI_HP_CMD_CLO         (1<<AHCI_HP_CMD_CLO_BIT)
#define AHCI_HP_CMD_POD         (1<<AHCI_HP_CMD_POD_BIT)
#define AHCI_HP_CMD_SUD         (1<<AHCI_HP_CMD_SUD_BIT)
#define AHCI_HP_CMD_ST          (1<<AHCI_HP_CMD_ST_BIT)

//
// hba_port_t::taskfile_data

#define AHCI_HP_TFD_ERR_BIT     8
#define AHCI_HP_TFD_SBSY_BIT    7
#define AHCI_HP_TFD_SCS64_BIT   4
#define AHCI_HP_TFD_SDRQ_BIT    3
#define AHCI_HP_TFD_SCS21_BIT   1
#define AHCI_HP_TFD_SERR_BIT    0

#define AHCI_HP_TFD_ERR_BITS    8
#define AHCI_HP_TFD_ERR_MASK    ((1<<AHCI_HP_TFD_ERR_BITS)-1)

#define AHCI_HP_TFD_ERR         (AHCI_HP_TFD_ERR_MASK << AHCI_HP_TFD_ERR_BIT)

typedef struct hba_fis_t {
    ahci_dma_setup_t dsfis;
    uint8_t pad0[4];

    ahci_pio_setup_t psfis;
    uint8_t pad1[12];

    // Register – Device to Host FIS
    ahci_fis_d2h_t rfis;
    uint8_t pad2[4];

    // 0x58 (not documented)
    uint8_t sdbfis[8];

    // Unknown fis buffer
    uint8_t ufis[64];

    uint8_t rsv[0x100-0xA0];
} hba_fis_t;

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
#define AHCI_CH_PMP_MASK    ((1<<AHCI_CH_PMP_BIT)-1)
#define AHCI_CH_PMP_n(n)    ((n) << AHCI_CH_PMP_BIT)

#define AHCI_CH_LEN_BITS    5
#define AHCI_CH_LEN_MASK    ((1<<AHCI_CH_LEN_BIT)-1)
#define AHCI_CH_LEN_n(n)    ((n) << AHCI_CH_LEN_BIT)

//
// hba_cmd_hdr_t::prdbc

// Data Byte Count
#define AHCI_CH_DBC_n(n)    (((n) << 1) | 1)

typedef struct hba_prdt_ent_t {
    // Data Base Address
    uint64_t dba;
    uint32_t rsv;
    // Data Byte Count
    uint32_t dbc_intr;
} hba_prdt_ent_t;

#define AHCI_PE_INTR_BIT    31
#define AHCI_PE_DBC_BIT     1
#define AHCI_PE_DBC_n(n)    (((n) << AHCI_PE_DBC_BIT) | 1)

typedef struct ahci_dev_t {
    pci_config_hdr_t config;
    hba_host_ctl_t volatile *mmio_base;
    hba_fis_t volatile *fis[32];
    hba_cmd_hdr_t volatile *cmd[32];
    hba_prdt_ent_t volatile *prd[32];
} ahci_dev_t;

#define AHCI_MAX_DEVICES    16
static ahci_dev_t ahci_devices[AHCI_MAX_DEVICES];
static unsigned ahci_count;

static int ahci_supports_64bit(ahci_dev_t *dev)
{
    return (dev->mmio_base->cap & AHCI_HC_CAP_S64A) != 0;
}

static void ahci_rebase(ahci_dev_t *dev)
{
    int addr_type = ahci_supports_64bit(dev)
            ? MAP_NOCACHE | MAP_WRITETHRU
            : MAP_32BIT | MAP_NOCACHE | MAP_WRITETHRU;

    // Loop through the implemented ports
    uint32_t impl = dev->mmio_base->ports_impl;
    for (int port_num = 0; port_num < 32; ++port_num) {
        if (!(impl & (1 << port_num)))
            continue;

        hba_port_t volatile *port = dev->mmio_base->ports + port_num;

        // Allocate space for FIS and command list
        hba_fis_t volatile *fis = mmap(
                    0, PAGESIZE,
                    PROT_READ | PROT_WRITE,
                    addr_type, -1, 0);
        hba_cmd_hdr_t volatile *cmd = mmap(
                    0, PAGESIZE,
                    PROT_READ | PROT_WRITE,
                    addr_type, -1, 0);
        hba_prdt_ent_t volatile *prd = mmap(
                    0, PAGESIZE,
                    PROT_READ | PROT_WRITE,
                    addr_type, -1, 0);

        dev->fis[port_num] = fis;
        dev->cmd[port_num] = cmd;
        dev->prd[port_num] = prd;

        port->cmd_list_base = mphysaddr((void*)cmd);
        port->fis_base = mphysaddr((void*)fis);

        if (port->sig != SATA_SIG_ATA)
            continue;

        fis->rfis.lba0 = 0;
        fis->rfis.lba2 = 0;
        fis->rfis.lba5 = 0;
        fis->rfis.count = 1;
        fis->rfis.fis_type = FIS_TYPE_REG_H2D;
        fis->rfis.error = 0;

        cmd->prdbc = 0;
        cmd->prdtl = 1;
        cmd->hdr = AHCI_CH_LEN_n(sizeof(fis->rfis));
        cmd->ctba = mphysaddr((void*)prd);

        port[port_num].cmd = AHCI_HP_CMD_ST;
    }
}

void ahci_init(void)
{
    pci_dev_iterator_t pci_iter;

    if (!pci_enumerate_begin(&pci_iter, 1, 6))
        return;

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

        if (ahci_count < countof(ahci_devices)) {
            ahci_devices[ahci_count].config = pci_iter.config;

            printk("SATA MMIO base %x\n", pci_iter.config.base_addr[5]);

            ahci_devices[ahci_count].mmio_base =
                    mmap((void*)(uintptr_t)pci_iter.config.base_addr[5],
                    0x1100, PROT_READ | PROT_WRITE,
                    MAP_PHYSICAL, -1, 0);

            printk("Ports implemented: %u\n",
                   ahci_devices[ahci_count].mmio_base->ports_impl);

            ahci_rebase(ahci_devices + ahci_count);

            ++ahci_count;
        }
        printk("\nIRQ line=%d, IRQ pin=%d\n",
               pci_iter.config.irq_line,
               pci_iter.config.irq_pin);
    } while (pci_enumerate_next(&pci_iter));
}
#endif
