#pragma once
#include "types.h"
#include "assert.h"

// Generic Address Structure
struct acpi_gas_t {
    uint8_t addr_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint32_t addr_lo;
    uint32_t addr_hi;
} _packed;

C_ASSERT(sizeof(acpi_gas_t) == 12);

#define ACPI_GAS_ADDR_SYSMEM    0
#define ACPI_GAS_ADDR_SYSIO     1
#define ACPI_GAS_ADDR_PCICFG    2
#define ACPI_GAS_ADDR_EMBED     3
#define ACPI_GAS_ADDR_SMBUS     4
#define ACPI_GAS_ADDR_FIXED     0x7F

#define ACPI_GAS_ASZ_UNDEF  0
#define ACPI_GAS_ASZ_8      1
#define ACPI_GAS_ASZ_16     2
#define ACPI_GAS_ASZ_32     3
#define ACPI_GAS_ASZ_64     4

//
// ACPI

// Root System Description Pointer (ACPI 1.0)
struct acpi_rsdp_t {
    char sig[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t rev;
    uint32_t rsdt_addr;
};

C_ASSERT(sizeof(acpi_rsdp_t) == 20);

// Root System Description Pointer (ACPI 2.0+)
struct acpi_rsdp2_t {
    acpi_rsdp_t rsdp1;

    uint32_t length;
    uint32_t xsdt_addr_lo;
    uint32_t xsdt_addr_hi;
    uint8_t checksum;
    uint8_t reserved[3];
};

C_ASSERT(sizeof(acpi_rsdp2_t) == 36);

// Root System Description Table
struct acpi_sdt_hdr_t {
    char sig[4];
    uint32_t len;
    uint8_t rev;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
} _packed;

C_ASSERT(sizeof(acpi_sdt_hdr_t) == 36);

// PCIe Enhanced Configuration Access Mechanism record in MCFG table
struct acpi_ecam_rec_t {
    uint64_t ecam_base;
    uint16_t segment_group;
    uint8_t st_bus;
    uint8_t en_bus;
    uint32_t reserved;
} _packed;

// MCFG table
struct acpi_mcfg_hdr_t {
    acpi_sdt_hdr_t hdr;
    uint64_t reserved;
    // followed by instances of acpi_ecam_record_t
} _packed;

// SRAT
struct acpi_srat_hdr_t {
    acpi_sdt_hdr_t hdr;
    uint8_t reserved[12];
} _packed;

C_ASSERT(sizeof(acpi_srat_hdr_t) == 48);

struct acpi_slit_t {
    acpi_sdt_hdr_t hdr;

    uint64_t locality_count;
    // Followed by locality_count squared 1-byte entries
} _packed;

struct acpi_srat_rec_hdr_t {
    uint8_t type;
    uint8_t len;
} _packed;

C_ASSERT(sizeof(acpi_srat_rec_hdr_t) == 2);

struct acpi_srat_lapic_t {
    acpi_srat_rec_hdr_t rec_hdr;

    uint8_t domain_lo;
    uint8_t apic_id;
    uint32_t flags;
    uint8_t sapic_eid;
    uint8_t domain_hi[3];
    uint32_t clk_domain;
} _packed;

C_ASSERT(sizeof(acpi_srat_lapic_t) == 16);

struct acpi_srat_mem_t {
    acpi_srat_rec_hdr_t rec_hdr;

    // Domain of the memory region
    uint32_t domain;

    uint8_t reserved1[2];

    // Range base and length
    uint64_t range_base;
    uint64_t range_length;

    uint8_t reserved2[4];

    // Only bit 0 is not reserved: 1=enabled
    uint32_t flags;
    uint8_t reserved3[8];
} _packed;

C_ASSERT(sizeof(acpi_srat_mem_t) == 40);

struct acpi_srat_x2apic_t {
    acpi_srat_rec_hdr_t rec_hdr;

    uint16_t reserved1;
    uint32_t domain;
    uint32_t x2apic_id;
    uint32_t flags;
    uint32_t clk_domain;
    uint32_t reserved2;
} _packed;

C_ASSERT(sizeof(acpi_srat_x2apic_t) == 24);

struct acpi_fadt_t {
    acpi_sdt_hdr_t hdr;
    uint32_t fw_ctl;
    uint32_t dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    uint8_t  reserved;

    uint8_t  preferred_pm_profile;
    uint16_t sci_irq;
    uint32_t smi_cmd_port;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4_bios_req;
    uint8_t  pstate_ctl;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_ctl_block;
    uint32_t pm1b_ctl_block;
    uint32_t pm2_ctl_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t  pm1_event_len;
    uint8_t  pm1_ctl_len;
    uint8_t  pm2_ctl_len;
    uint8_t  pm_timer_len;
    uint8_t  gpe0_len;
    uint8_t  gpe1_len;
    uint8_t  gpe1_base;
    uint8_t  cstate_ctl;
    uint16_t worst_c2_lat;
    uint16_t worst_c3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_ofs;
    uint8_t  duty_width;
    uint8_t  day_alarm;
    uint8_t  month_alarm;
    uint8_t  century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    uint16_t boot_arch_flags;

    uint8_t  reserved2;

    // ACPI_FADT_FFF_*
    uint32_t flags;

    // 12 byte structure; see below for details
    acpi_gas_t reset_reg;

    uint8_t  reset_value;
    uint8_t  reserved3[3];

    // 64bit pointers - Available on ACPI 2.0+
    uint64_t x_fw_ctl;
    uint64_t x_dsdt;

    acpi_gas_t x_pm1a_event_block;
    acpi_gas_t x_pm1b_event_block;
    acpi_gas_t x_pm1a_control_block;
    acpi_gas_t x_pm1b_control_block;
    acpi_gas_t x_pm2Control_block;
    acpi_gas_t x_pm_timer_block;
    acpi_gas_t x_gpe0_block;
    acpi_gas_t x_gpe1_block;
} _packed;

struct acpi_ssdt_t {
    // sig == ?
    acpi_sdt_hdr_t hdr;
} _packed;

struct acpi_madt_rec_hdr_t {
    uint8_t entry_type;
    uint8_t record_len;
};

struct acpi_madt_lapic_t {
    acpi_madt_rec_hdr_t hdr;
    uint8_t cpu_id;
    uint8_t apic_id;
    uint32_t flags;
} _packed;

struct acpi_madt_ioapic_t {
    acpi_madt_rec_hdr_t hdr;
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t addr;
    uint32_t irq_base;
} _packed;

struct acpi_madt_irqsrc_t {
    acpi_madt_rec_hdr_t hdr;
    uint8_t bus;
    uint8_t irq_src;
    uint32_t gsi;
    uint16_t flags;
} _packed;

// Which IOAPIC inputs should be NMI
struct acpi_madt_nmisrc_t {
    acpi_madt_rec_hdr_t hdr;
    uint16_t flags;
    uint32_t gsi;
} _packed;

struct acpi_madt_lnmi_t {
    acpi_madt_rec_hdr_t hdr;
    uint8_t apic_id;
    uint16_t flags;
    uint8_t lapic_lint;
} _packed;

struct acpi_madt_x2apic_t {
    acpi_madt_rec_hdr_t hdr;
    uint16_t reserved;
    uint32_t x2apic_id;
    uint32_t flags;
    uint32_t uid;
} _packed;

//
// The IRQ routing flags are identical to MPS flags

union acpi_madt_ent_t {
    acpi_madt_rec_hdr_t hdr;

    // ACPI_MADT_REC_TYPE_LAPIC
    acpi_madt_lapic_t lapic;

    // ACPI_MADT_REC_TYPE_IOAPIC
    acpi_madt_ioapic_t ioapic;

    // ACPI_MADT_REC_TYPE_IRQ
    acpi_madt_irqsrc_t irq_src;

    // ACPI_MADT_REC_TYPE_NMI
    acpi_madt_nmisrc_t nmi_src;

    // ACPI_MADT_REC_TYPE_LNMI
    acpi_madt_lnmi_t lnmi;

    // ACPI_MADT_REC_TYPE_X2APIC
    acpi_madt_x2apic_t x2apic;
} _packed;

struct acpi_madt_t {
    // sig == "APIC"
    acpi_sdt_hdr_t hdr;

    uint32_t lapic_address;

    // 1 = Dual 8259 PICs installed
    uint32_t flags;
} _packed;

//
// HPET ACPI info

struct acpi_hpet_t {
    acpi_sdt_hdr_t hdr;

    uint32_t blk_id;
    acpi_gas_t addr;
    uint8_t number;
    uint16_t min_tick_count;
    uint8_t page_prot;
};

#define ACPI_HPET_BLKID_PCI_VEN_BIT     16
#define ACPI_HPET_BLKID_LEGACY_CAP_BIT  15
#define ACPI_HPET_BLKID_COUNTER_SZ_BIT  13
#define ACPI_HPET_BLKID_NUM_CMP_BIT     8
#define ACPI_HPET_BLKID_REV_ID_BIT      0

#define ACPI_HPET_BLKID_PCI_VEN_BITS    16
#define ACPI_HPET_BLKID_NUM_CMP_BITS    8
#define ACPI_HPET_BLKID_REV_ID_BITS     8


static _always_inline uint8_t checksum_bytes(char const *bytes, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
        sum += (uint8_t)bytes[i];
    return sum;
}

static _always_inline uint8_t acpi_chk_hdr(acpi_sdt_hdr_t *hdr)
{
    return checksum_bytes((char const *)hdr, hdr->len);
}
