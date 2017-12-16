#pragma once
#include "types.h"
#include "assert.h"
#include "bswap.h"
#include "printk.h"

enum struct ata_cmd_t : uint8_t {
    NOP                 = 0x00,

    // NCQ DMA
    READ_DMA_NCQ        = 0x60,
    WRITE_DMA_NCQ       = 0x61,

    // PIO
    READ_PIO            = 0x20,
    WRITE_PIO           = 0x30,
    READ_PIO_EXT        = 0x24,
    WRITE_PIO_EXT       = 0x34,

    // Multiple
    READ_MULT_PIO       = 0xC5,
    WRITE_MULT_PIO      = 0xC4,
    READ_MULT_PIO_EXT   = 0x29,
    WRITE_MULT_PIO_EXT  = 0x39,
    WRITE_MULT_FUA_EXT  = 0xCE,

    // DMA
    READ_DMA            = 0xC8,
    WRITE_DMA           = 0xCA,
    READ_DMA_EXT        = 0x25,
    WRITE_DMA_EXT       = 0x35,
    WRITE_DMA_FUA_EXT   = 0x3D,

    // Cache flush
    CACHE_FLUSH         = 0xE7,
    CACHE_FLUSH_EXT     = 0xEA,

    // Read drive capabilities
    IDENTIFY            = 0xEC,
    IDENTIFY_PACKET     = 0xA1,

    // ATAPI command packet
    PACKET              = 0xA0,

    // Configure
    SET_MULTIPLE        = 0xC6,
    SET_FEATURE         = 0xEF,
    SET_MULT_MODE       = 0xC6,

    // Log
    READ_LOG_EXT        = 0x2F,
    WRITE_LOG_EXT       = 0x3F,
    READ_LOG_DMA_EXT    = 0x47,
    WRITE_LOG_DMA_EXT   = 0x57,

    // Compact flash
    CFA_XLAT_SECT       = 0x87,
    CFA_ERASE_SECT      = 0xC0,
    CFA_WRITE_NE        = 0x38,
    CFA_WRITE_MULT_NE   = 0xCD,
    CFA_REQ_EXT_ERR     = 0x03,
    CFA_MEDIA_CARD_TYPE = 0xD1,

    // Stream
    READ_STR_EXT        = 0x2B,
    WRITE_STR_DMA       = 0x3B,
    READ_STR_DMA_EXT    = 0x2A,
    WRITE_STR_DMA_EXT   = 0x3A,

    // Verify
    VERIFY_SECTORS      = 0x40,
    VERIFY_SECTORS_EXT  = 0x42,

    // Trusted
    TRUSTED_NONDATA     = 0x5B,
    TRUSTED_RECV        = 0x5C,
    TRUSTED_RECV_DMA    = 0x5D,
    TRUSTED_SEND        = 0x5E,
    TRUSTED_SEND_DMA    = 0x5F,

    // Power management
    STANDBY_IMM         = 0xE0,
    IDLE_IMM            = 0xE1,
    STANDBY             = 0xE2,
    IDLE                = 0xE3,
    SLEEP               = 0xE6,
    CHK_PWR_MODE        = 0xE5,

    // Buffer
    WRITE_BUFFER_DMA    = 0xE8,
    READ_BUFFER_DMA     = 0xE9,
    READ_BUFFER         = 0xE4,

    // Trim
    DATA_SET_MGMT       = 0x06,

    // Security
    SEC_SET_PASSWORD    = 0xF1,
    SEC_UNLOCK          = 0xF2,
    SEC_ERASE_PREP      = 0xF3,
    SEC_ERASE_UNIT      = 0xF4,
    SEC_FREEZE_LOCK     = 0xF5,
    SEC_DIS_PASSWORD    = 0xF6,
    SANITIZE            = 0xB4,

    // Max address
    SET_MAX_ADDR_EXT    = 0x37,
    SET_MAX_ADDR        = 0xF9,

    // Firmware
    MICROCODE           = 0x92,
    MICROCODE_DMA       = 0x93,

    // Diagnostic
    DIAGNOSTIC          = 0x90,
    WRITE_UNCORR_EXT    = 0x45,
    SMART               = 0xB0,

    DEVICE_RESET        = 0x08,
    REQ_SENSE_EXT       = 0x0B,
    READ_N_MAX_ADDR_EXT = 0x27,
    CONFIG_STR          = 0x51,
    NV_CACHE            = 0xB6,
    READ_N_MAX_ADDR     = 0xF8,
};

struct ata_identify_t {
    // Word 0
    uint16_t atapi_packet_size:2;   // 00=12, 01=16
    uint16_t incomplete:1;
    uint16_t :4;
    uint16_t removable:1;
    uint16_t :6;
    uint16_t atapi_device:2;  // 10=ATAPI, 0x=ATA

    // Word 1
    uint16_t unused_1;

    // Word 2
    uint16_t specific_config;

    // Word 3-9
    uint16_t unused_3_9[10-3];

    // Word 10-19
    char serial[(20-10)*2];

    // Word 20-22
    uint16_t unused_20_22[23-20];

    // Word 23-26
    char fw_revision[(27-23)*2];

    // Word 27-46
    char model[(47-27)*2];

    // Word 47
    uint16_t max_multiple:8;
    uint16_t :8;    // always 0x80

    // Word 48
    uint16_t support_trusted:1;
    uint16_t :15;

    // Word 49
    uint16_t current_long_phys_alignment:2;
    uint16_t :6;
    uint16_t dma_supported:1;
    uint16_t lba_supported:1;
    uint16_t iordy_may_be_disabled:1;
    uint16_t support_iordy:1;
    uint16_t :1;
    uint16_t std_sby:1;
    uint16_t :2;

    // Word 50
    uint16_t :1;    // 0
    uint16_t :1;    // 1
    uint16_t :13;
    uint16_t dev_spec_min_sby:1;

    // Word 51-52
    uint16_t unused_51_52[53-51];

    // Word 53
    uint16_t :1;
    uint16_t word_70_64_valid:1;
    uint16_t word_88_valid:1;
    uint16_t :5;
    uint16_t freefall_sensitivity:8;

    // Word 54-58
    uint16_t unused_54_58[59-54];

    // Word 59
    uint16_t current_mult:8;
    uint16_t current_mult_valid:1;
    uint16_t :3;
    uint16_t support_sanitize:1;
    uint16_t support_crypto_scramble:1;
    uint16_t support_overwrite_ext:1;
    uint16_t support_block_erase_ext:1;

    // Word 60-61
    uint32_t max_lba;

    // Word 62
    uint16_t unused_62;

    // Word 63
    uint16_t mwdma_support:3;
    uint16_t :5;
    uint16_t mwdma_selected:3;
    uint16_t :5;

    // Word 64
    uint16_t pio_support:8;
    uint16_t :8;

    // Word 65
    uint16_t mwdma_min_ns;

    // Word 66
    uint16_t mwdma_rec_ns;

    // Word 67
    uint16_t min_pio_noflow_ns;

    // Word 68
    uint16_t min_pio_iordy_ns;

    // Word 69-74
    uint16_t unused_69_74[75-69];

    // Word 75
    uint16_t max_queue_minus1:5;
    uint16_t :11;

    // Word 76 (SATA)
    uint16_t :1;
    uint16_t support_sata_gen1:1;
    uint16_t support_sata_gen2:1;
    uint16_t :5;
    uint16_t support_ncq:1;
    uint16_t support_host_pm:1;
    uint16_t support_phy_event_counters:1;
    uint16_t support_unload_during_ncq:1;
    uint16_t support_ncq_priority:1;
    uint16_t :3;

    // Word 77
    uint16_t unused_77;

    // Word 78 (SATA features supported)
    uint16_t :1;
    uint16_t support_nonzero_buffer_offset:1;
    uint16_t support_dma_setup_auto_activate:1;
    uint16_t support_initiating_pm:1;
    uint16_t support_inorder_data_delivery:1;
    uint16_t :1;
    uint16_t support_setting_preservation:1;
    uint16_t :9;

    // Word 79
    uint16_t :1;
    uint16_t nonzero_buffer_offset_enabled:1;
    uint16_t dma_setup_auto_activate_enabled:1;
    uint16_t initiating_pm_enabled:1;
    uint16_t inorder_data_delivery_enabled:1;
    uint16_t :1;
    uint16_t setting_preservation_enabled:1;
    uint16_t :9;

    // Word 80
    uint16_t :3;
    uint16_t support_ata3:1;
    uint16_t support_ata4:1;
    uint16_t support_ata5:1;
    uint16_t support_ata6:1;
    uint16_t :9;

    // Word 81
    uint16_t minor_ver;

    // Word 82
    uint16_t support_smart:1;
    uint16_t support_security:1;
    uint16_t support_removable:1;
    uint16_t support_pm:1;
    uint16_t support_packet:1;
    uint16_t support_writecache:1;
    uint16_t support_lookahead:1;
    uint16_t support_release_irq:1;
    uint16_t support_service_irq:1;
    uint16_t support_device_reset:1;
    uint16_t support_hpa:1;
    uint16_t :1;
    uint16_t support_write_buffer:1;
    uint16_t support_read_buffer:1;
    uint16_t support_nop:1;
    uint16_t :1;

    // Word 83
    uint16_t support_microcode:1;
    uint16_t support_dma_queued:1;
    uint16_t support_cfa:1;
    uint16_t support_apm:1;
    uint16_t support_removable_notify:1;
    uint16_t support_puis:1;
    uint16_t support_set_features_spinup:1;
    uint16_t support_addr_offset_rsvd:1;
    uint16_t support_set_max:1;
    uint16_t support_acoustic:1;
    uint16_t support_ext48bit:1;
    uint16_t support_cfg_overlay:1;
    uint16_t support_flush_cache:1;
    uint16_t support_flush_cache_ext:1;
    uint16_t :2;

    // Word 84
    uint16_t support_smart_log:1;
    uint16_t support_smart_test:1;
    uint16_t support_media_serial:1;
    uint16_t support_media_card:1;
    uint16_t support_streaming:1;
    uint16_t support_logging:1;
    uint16_t support_fua_ext:1;
    uint16_t :1;
    uint16_t support_wwn:1;
    uint16_t :4;
    uint16_t support_idle_imm_unload:1;
    uint16_t :2;

    // Word 85
    uint16_t smart_enabled:1;
    uint16_t security_enabled:1;
    uint16_t removable_enabled:1;
    uint16_t pm_enabled:1;
    uint16_t packet_enabled:1;
    uint16_t write_cache_enabled:1;
    uint16_t lookahead_enabled:1;
    uint16_t release_irq_enabled:1;
    uint16_t service_irq_enabled:1;
    uint16_t device_reset_enabled:1;
    uint16_t hpa_enabled:1;
    uint16_t :1;
    uint16_t write_buffer_enabled:1;
    uint16_t read_buffer_enabled:1;
    uint16_t nop_enabled:1;
    uint16_t :1;

    // Word 86
    uint16_t microcode_enabled:1;
    uint16_t dma_queued_enabled:1;
    uint16_t cfa_enabled:1;
    uint16_t apm_enabled:1;
    uint16_t removable_notify_enabled:1;
    uint16_t puis_enabled:1;
    uint16_t set_features_spinup_enabled:1;
    uint16_t addr_offset_rsvd_enabled:1;
    uint16_t set_max_enabled:1;
    uint16_t acoustic_enabled:1;
    uint16_t ext48bit_enabled:1;
    uint16_t cfg_overlay_enabled:1;
    uint16_t flush_cache_enabled:1;
    uint16_t flush_cache_ext_enabled:1;
    uint16_t :2;

    // Word 87
    uint16_t smart_default_enabled:1;
    uint16_t smart_test_default_enabled:1;
    uint16_t media_serial_valid:1;
    uint16_t media_card_default_enabled:1;
    uint16_t :1;
    uint16_t logging_default_enabled:1;
    uint16_t :10;

    // Word 88
    uint16_t udma_support:6;
    uint16_t :2;
    uint16_t udma_selected:6;
    uint16_t :2;

    // Word 89
    uint16_t secure_erase_time;

    // Word 90
    uint16_t enhanced_erase_time;

    // Word 91
    uint16_t current_apm;

    // Word 92
    uint16_t master_password_rev_code;

    // Word 93
    uint16_t :1;
    uint16_t master_dev_number_source:2;
    uint16_t master_passed_diag:1;
    uint16_t master_detected_pdiag:1;
    uint16_t master_detected_dasp:1;
    uint16_t master_both_respond:1;
    uint16_t :1;
    uint16_t :1;
    uint16_t slave_dev_number_source:2;
    uint16_t slave_detected_pdiag:1;
    uint16_t :1;
    uint16_t cable_id:1;
    uint16_t :2;

    // Word 94
    uint16_t current_acoustic:8;
    uint16_t recommended_acoustic:8;

    // Word 95-99
    uint16_t unused_95_99[100-95];

    // Word 100-103
    uint64_t max_lba_ext48bit;

    // Word 104
    uint16_t streaming_xfer_time_pio;

    // Word 105
    uint16_t max_lba_range_entries;

    // Word 106
    uint16_t log2_sectors_per_phys_sector:4;
    uint16_t :8;
    uint16_t sector_longer_than_512:1;
    uint16_t multiple_log_sectors_per_phys:1;
    uint16_t :2;

    // Word 107
    uint16_t interseek_delay_for_am;

    // Word 108-111
    uint16_t worldwide_name[112-108];

    // Word 112-116
    uint16_t unused_104_126[117-112];

    // Word 117-118
    uint32_t logical_sector_size;

    // Word 119-126
    uint16_t unused_119_126[127-119];

    // Word 127
    uint16_t removable_notify_support:2;
    uint16_t :14;

    // Word 128
    uint16_t security_supported:1;
    uint16_t security_enabled_2:1;
    uint16_t security_locked:1;
    uint16_t security_frozen:1;
    uint16_t security_count_expired:1;
    uint16_t enhanced_erase_supported:1;
    uint16_t :2;
    uint16_t security_level:1;
    uint16_t :7;

    // Word 129-159
    uint16_t vendor_specific[160-129];

    // Word 160
    uint16_t max_current_ma:12;
    uint16_t cfa_mode1_disabled:1;
    uint16_t cfa_mode1_required:1;
    uint16_t :1;
    uint16_t word_160_supported:1;

    // Word 161-167
    uint16_t cf_reserved[168-161];

    // Word 168
    uint16_t nominal_form_factor:4;
    uint16_t :12;

    // Word 169
    uint16_t support_trim:1;
    uint16_t :15;

    // Word 170-173
    char extra_product_id[(174-170)*2];

    // Word 174-175
    uint16_t unused_174_175[176-174];

    // Word 176-205
    char media_serial[(206-176)*2];

    // Word 206-254
    uint16_t reserved[255-206];

    uint16_t signature:8;
    uint16_t checksum:8;

    void fixup_strings()
    {
        htons_buf(serial, sizeof(serial));
        htons_buf(fw_revision, sizeof(fw_revision));
        htons_buf(model, sizeof(model));
        htons_buf(media_serial, sizeof(media_serial));
    }
} __packed;

C_ASSERT(offsetof(ata_identify_t, unused_1) == 1*2);
C_ASSERT(offsetof(ata_identify_t, unused_3_9) == 3*2);
C_ASSERT(offsetof(ata_identify_t, unused_20_22) == 20*2);
C_ASSERT(offsetof(ata_identify_t, unused_51_52) == 51*2);
C_ASSERT(offsetof(ata_identify_t, unused_54_58) == 54*2);
C_ASSERT(offsetof(ata_identify_t, unused_62) == 62*2);
C_ASSERT(offsetof(ata_identify_t, unused_69_74) == 69*2);
C_ASSERT(offsetof(ata_identify_t, minor_ver) == 81*2);
C_ASSERT(offsetof(ata_identify_t, unused_95_99) == 95*2);
C_ASSERT(offsetof(ata_identify_t, secure_erase_time) == 89*2);
C_ASSERT(offsetof(ata_identify_t, max_lba_ext48bit) == 100*2);
C_ASSERT(offsetof(ata_identify_t, vendor_specific) == 129*2);
C_ASSERT(sizeof(ata_identify_t) == 512);

struct atapi_fis_t {
    uint8_t op;

    uint8_t dma;

    uint8_t lba3;
    uint8_t lba2;
    uint8_t lba1;
    uint8_t lba0;

    uint8_t len3;
    uint8_t len2;
    uint8_t len1;
    uint8_t len0;

    uint8_t zero2;

    uint8_t control;

    uint32_t zero3;

    void set(uint8_t operation, uint32_t lba, uint32_t len, uint8_t use_dma)
    {
        op = operation;
        set_lba(lba);
        set_len(len);
        dma = use_dma;
        zero2 = 0;
        control = 0;
        zero3 = 0;
    }

    void set_lba(uint32_t lba)
    {
        lba3 = (lba >> (3*8)) & 0xFF;
        lba2 = (lba >> (2*8)) & 0xFF;
        lba1 = (lba >> (1*8)) & 0xFF;
        lba0 = (lba >> (0*8)) & 0xFF;
    }

    void set_len(uint32_t len)
    {
        len3 = (len >> (3*8)) & 0xFF;
        len2 = (len >> (2*8)) & 0xFF;
        len1 = (len >> (1*8)) & 0xFF;
        len0 = (len >> (0*8)) & 0xFF;
    }
};

C_ASSERT(sizeof(atapi_fis_t) == 16);

//
// ATAPI commands

#define ATAPI_CMD_READ              0xA8
#define ATAPI_IDENTIFY_PACKET       0xA1
#define ATAPI_CMD_EJECT             0x1B

#define ATA_REG_STATUS_ERR_BIT      0
#define ATA_REG_STATUS_IDX_BIT      1
#define ATA_REG_STATUS_CORR_BIT     2
#define ATA_REG_STATUS_DRQ_BIT      3
#define ATA_REG_STATUS_DSC_BIT      4
#define ATA_REG_STATUS_DWF_BIT      5
#define ATA_REG_STATUS_DRDY_BIT     6
#define ATA_REG_STATUS_BSY_BIT      7

#define ATA_REG_STATUS_ERR          (1U<<ATA_REG_STATUS_ERR_BIT)
#define ATA_REG_STATUS_IDX          (1U<<ATA_REG_STATUS_IDX_BIT)
#define ATA_REG_STATUS_CORR         (1U<<ATA_REG_STATUS_CORR_BIT)
#define ATA_REG_STATUS_DRQ          (1U<<ATA_REG_STATUS_DRQ_BIT)
#define ATA_REG_STATUS_DSC          (1U<<ATA_REG_STATUS_DSC_BIT)
#define ATA_REG_STATUS_DWF          (1U<<ATA_REG_STATUS_DWF_BIT)
#define ATA_REG_STATUS_DRDY         (1U<<ATA_REG_STATUS_DRDY_BIT)
#define ATA_REG_STATUS_BSY          (1U<<ATA_REG_STATUS_BSY_BIT)

__used
static format_flag_info_t const ide_flags_status[] = {
    { "ERR",  1, 0, ATA_REG_STATUS_ERR_BIT  },
    { "IDX",  1, 0, ATA_REG_STATUS_IDX_BIT  },
    { "CORR", 1, 0, ATA_REG_STATUS_CORR_BIT },
    { "DRQ",  1, 0, ATA_REG_STATUS_DRQ_BIT  },
    { "DSC",  1, 0, ATA_REG_STATUS_DSC_BIT  },
    { "DWF",  1, 0, ATA_REG_STATUS_DWF_BIT  },
    { "DRDY", 1, 0, ATA_REG_STATUS_DRDY_BIT },
    { "BSY",  1, 0, ATA_REG_STATUS_BSY_BIT  },
    { 0,      0, 0, -1,                     }
};

#define ATA_REG_CONTROL_nIEN_BIT    1
#define ATA_REG_CONTROL_SRST_BIT    2

#define ATA_REG_CONTROL_nIEN        (1U<<ATA_REG_CONTROL_nIEN_BIT)
#define ATA_REG_CONTROL_SRST        (1U<<ATA_REG_CONTROL_SRST_BIT)

#define ATA_REG_CONTROL_n(n)        (0x08 | (n))

#define ATA_REG_HDDEVSEL_HD_BIT     0
#define ATA_REG_HDDEVSEL_HD_BITS    4
#define ATA_REG_HDDEVSEL_DRV_BIT    4
#define ATA_REG_HDDEVSEL_LBA_BIT    6

#define ATA_REG_HDDEVSEL_HD_MASK    ((1U<<ATA_REG_HDDEVSEL_HD_BITS)-1)

#define ATA_REG_HDDEVSEL_HD \
    (ATA_REG_HDDEVSEL_HD_MASK<<ATA_REG_HDDEVSEL_HD_BIT)
#define ATA_REG_HDDEVSEL_HD_n(n) \
    ((n)<<ATA_REG_HDDEVSEL_HD_BIT)
#define ATA_REG_HDDEVSEL_DRV        (1U<<ATA_REG_HDDEVSEL_DRV_BIT)
#define ATA_REG_HDDEVSEL_LBA        (1U<<ATA_REG_HDDEVSEL_LBA_BIT)
#define ATA_REG_HDDEVSEL_n(n)       ((n)|0xA0U)

#define ATA_REG_ERROR_AMNF_BIT      0
#define ATA_REG_ERROR_TKONF_BIT     1
#define ATA_REG_ERROR_ABRT_BIT      2
#define ATA_REG_ERROR_MCR_BIT       3
#define ATA_REG_ERROR_IDNF_BIT      4
#define ATA_REG_ERROR_MC_BIT        5
#define ATA_REG_ERROR_UNC_BIT       6
#define ATA_REG_ERROR_BBK_BIT       7

extern format_flag_info_t const ide_flags_error[];

// Address mark not found
#define ATA_REG_ERROR_AMNF          (1U<<ATA_REG_ERROR_AMNF_BIT)

// Track 0 not found
#define ATA_REG_ERROR_TKONF         (1U<<ATA_REG_ERROR_TKONF_BIT)

// Aborted command
#define ATA_REG_ERROR_ABRT          (1U<<ATA_REG_ERROR_ABRT_BIT)

// Media Change Requested
#define ATA_REG_ERROR_MCR           (1U<<ATA_REG_ERROR_MCR_BIT)

// ID not found
#define ATA_REG_ERROR_IDNF          (1U<<ATA_REG_ERROR_IDNF_BIT)

// Media changed
#define ATA_REG_ERROR_MC            (1U<<ATA_REG_ERROR_MC_BIT)

// Uncorrectable data error
#define ATA_REG_ERROR_UNC           (1U<<ATA_REG_ERROR_UNC_BIT)

// Bad block detected
#define ATA_REG_ERROR_BBK           (1U<<ATA_REG_ERROR_BBK_BIT)

//
// Features

// 01h Enable 8-bit PIO transfer mode (CFA feature set only)
#define ATA_FEATURE_ENA_8BIT_PIO    0x01

// 02h Enable write cache
#define ATA_FEATURE_ENA_WR_CACHE    0x02

// 03h Set transfer mode based on value in Sector Count register.
#define ATA_FEATURE_XFER_MODE       0x03

// 05h Enable advanced power management
#define ATA_FEATURE_ENABLE_APM      0x05

// 06h Enable Power-Up In Standby feature set.
#define ATA_FEATURE_ENA_PUIS        0x06

// 07h Power-Up In Standby feature set device spin-up.
#define ATA_FEATURE_PUIS_FS         0x07

// 0Ah Enable CFA power mode 1
#define ATA_FEATURE_CFA_PWR_M1      0x0A

// 31h Disable Media Status Notification
#define ATA_FEATURE_DIS_MSN         0x31

// 42h Enable Automatic Acoustic Management feature set
#define ATA_FEATURE_ENA_AAM         0x42

// 55h Disable read look-ahead feature
#define ATA_FEATURE_DIS_RLA         0x55

// 5Dh Enable release interrupt
#define ATA_FEATURE_ENA_REL_INTR    0x5D

// 5Eh Enable SERVICE interrupt
#define ATA_FEATURE_ENA_SVC_INTR    0x5E

// 66h Disable reverting to power-on defaults
#define ATA_FEATURE_DIS_POD         0x66

// 81h Disable 8-bit PIO transfer mode (CFA feature set only)
#define ATA_FEATURE_DIS_8BIT_PIO    0x81

// 82h Disable write cache
#define ATA_FEATURE_DIS_WR_CACHE    0x82

// 85h Disable advanced power management
#define ATA_FEATURE_DIS_APM         0x85

// 86h Disable Power-Up In Standby feature set.
#define ATA_FEATURE_DIS_PUIS        0x86

// 8Ah Disable CFA power mode 1
#define ATA_FEATURE_DIS_CFA_M1      0x8A

// 95h Enable Media Status Notification
#define ATA_FEATURE_ENA_MSN         0x95

// AAh Enable read look-ahead feature
#define ATA_FEATURE_ENA_RLA         0xAA

// C2h Disable Automatic Acoustic Management feature set
#define ATA_FEATURE_DIS_AAM         0xC2

// CCh Enable reverting to power-on defaults
#define ATA_FEATURE_ENA_POD         0xCC

// DDh Disable release interrupt
#define ATA_FEATURE_DIS_REL_INTR    0xDD

// DEh Disable SERVICE interrupt
#define ATA_FEATURE_DIS_SVC_INTR    0xDE

#define ATA_FEATURE_XFER_MODE_UDMA_n(n) (0x40 | (n))
#define ATA_FEATURE_XFER_MODE_MWDMA_n(n) (0x20 | (n))
