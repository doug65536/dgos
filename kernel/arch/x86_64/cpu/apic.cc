#include "apic.h"
#include "cpu/asm_constants.h"
#include "device/acpigas.h"
#include "types.h"
#include "gdt.h"
#include "bios_data.h"
#include "control_regs.h"
#include "interrupts.h"
#include "irq.h"
#include "isr.h"
#include "thread_impl.h"
#include "mm.h"
#include "cpuid.h"
#include "string.h"
#include "atomic.h"
#include "printk.h"
#include "likely.h"
#include "time.h"
#include "cpuid.h"
#include "assert.h"
#include "bitsearch.h"
#include "callout.h"
#include "device/pci.h"
#include "ioport.h"
#include "mm.h"
#include "vector.h"
#include "bootinfo.h"
#include "nano_time.h"
#include "cmos.h"
#include "apic.bits.h"
#include "mutex.h"
#include "bootinfo.h"
#include "boottable.h"
#include "inttypes.h"

#define ENABLE_ACPI 1

#define DEBUG_ACPI  1
#if DEBUG_ACPI
#define ACPI_TRACE(...) printk("acpi: " __VA_ARGS__)
#else
#define ACPI_TRACE(...) ((void)0)
#endif

#define ACPI_ERROR(...) printk("mp: error: " __VA_ARGS__)

#define DEBUG_MPS  1
#if DEBUG_MPS
#define MPS_TRACE(...) printk("mp: " __VA_ARGS__)
#else
#define MPS_TRACE(...) ((void)0)
#endif

#define MPS_ERROR(...) printk("mp: " __VA_ARGS__)

#define DEBUG_APIC  1
#if DEBUG_APIC
#define APIC_TRACE(...) printk("lapic: " __VA_ARGS__)
#else
#define APIC_TRACE(...) ((void)0)
#endif

#define APIC_ERROR(...) printk("lapic: error: " __VA_ARGS__)

#define DEBUG_IOAPIC  1
#if DEBUG_IOAPIC
#define IOAPIC_TRACE(...) printk("ioapic: " __VA_ARGS__)
#else
#define IOAPIC_TRACE(...) ((void)0)
#endif

//
// MP Tables

struct mp_table_hdr_t {
    char sig[4];
    uint32_t phys_addr;
    uint8_t length;
    uint8_t spec;
    uint8_t checksum;
    uint8_t features[5];
};

struct mp_cfg_tbl_hdr_t {
    char sig[4];
    uint16_t base_tbl_len;
    uint8_t spec_rev;
    uint8_t checksum;
    char oem_id_str[8];
    char prod_id_str[12];
    uint32_t oem_table_ptr;
    uint16_t oem_table_size;
    uint16_t entry_count;
    uint32_t apic_addr;
    uint16_t ext_tbl_len;
    uint8_t ext_tbl_checksum;
    uint8_t reserved;
};

// entry_type 0
struct mp_cfg_cpu_t {
    uint8_t entry_type;
    uint8_t apic_id;
    uint8_t apic_ver;
    uint8_t flags;
    uint32_t signature;
    uint32_t features;
    uint32_t reserved1;
    uint32_t reserved2;
};

// entry_type 1
struct mp_cfg_bus_t {
    uint8_t entry_type;
    uint8_t bus_id;
    char type[6];
};

// entry_type 2
struct mp_cfg_ioapic_t {
    uint8_t entry_type;
    uint8_t id;
    uint8_t ver;
    uint8_t flags;
    uint32_t addr;
};

//
// IOAPIC registers

#define IOAPIC_IOREGSEL         0
#define IOAPIC_IOREGWIN         4

#define IOAPIC_REG_ID           0
#define IOAPIC_REG_VER          1
#define IOAPIC_REG_ARB          2

#define IOAPIC_RED_LO_n(n)      (0x10 + (n) * 2)
#define IOAPIC_RED_HI_n(n)      (0x10 + (n) * 2 + 1)

static char const * const intr_type_text[] = {
    "APIC",
    "NMI",
    "SMI",
    "EXTINT"
};

// entry_type 3
struct mp_cfg_iointr_t {
    uint8_t entry_type;
    uint8_t type;
    uint16_t flags;
    uint8_t source_bus;
    uint8_t source_bus_irq;
    uint8_t dest_ioapic_id;
    uint8_t dest_ioapic_intin;
};

// entry_type 4
struct mp_cfg_lintr_t {
    uint8_t entry_type;
    uint8_t type;
    uint16_t flags;
    uint8_t source_bus;
    uint8_t source_bus_irq;
    uint8_t dest_lapic_id;
    uint8_t dest_lapic_lintin;
};

// entry_type 128
struct mp_cfg_addrmap_t {
    uint8_t entry_type;
    uint8_t len;
    uint8_t bus_id;
    uint8_t addr_type;
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint32_t len_lo;
    uint32_t len_hi;
};

// entry_type 129
struct mp_cfg_bushier_t {
    uint8_t entry_type;
    uint8_t len;
    uint8_t bus_id;
    uint8_t info;
    uint8_t parent_bus;
    uint8_t reserved[3];
};

// entry_type 130
struct mp_cfg_buscompat_t {
    uint8_t entry_type;
    uint8_t len;
    uint8_t bus_id;
    uint8_t bus_mod;
    uint32_t predef_range_list;
};

struct mp_ioapic_t {
    uint8_t id;
    uint8_t base_intr;
    uint8_t vector_count;
    uint8_t irq_base;
    uint32_t addr;
    uint32_t volatile *ptr;
    using lock_type = std::mcslock;
    using scoped_lock = std::unique_lock<std::mcslock>;
    lock_type lock;
};

static char const *mp_tables;

static std::vector<uint8_t> mp_pci_bus_ids;
static uint16_t mp_isa_bus_id;

static uint64_t apic_timer_freq;

static unsigned ioapic_count;
static mp_ioapic_t ioapic_list[16];

using ioapic_msi_alloc_lock_type = std::mcslock;
using ioapic_msi_alloc_scoped_lock =
    std::unique_lock<ioapic_msi_alloc_lock_type>;
static ioapic_msi_alloc_lock_type ioapic_msi_alloc_lock;

// Bit 0 of this corresponds to vector INTR_APIC_IRQ_BASE
static uint64_t ioapic_msi_alloc_map[] = {
    0x0000000000000000L,
    0x0000000000000000L,
    0x0000000000000000L,
    0x0000000000000000L
};

static mp_ioapic_t *ioapic_by_id(uint8_t id);
static void ioapic_reset(mp_ioapic_t *ioapic);
static void ioapic_set_type(mp_ioapic_t *ioapic,
                            uint8_t intin, uint8_t intr_type);
static void ioapic_set_flags(mp_ioapic_t *ioapic,
                             uint8_t intin, uint16_t intr_flags, bool isa);
static uint32_t ioapic_read(mp_ioapic_t *ioapic, uint32_t reg,
                            mp_ioapic_t::scoped_lock const &);
static void ioapic_write(
        mp_ioapic_t *ioapic, uint32_t reg, uint32_t value,
        mp_ioapic_t::scoped_lock const&);

static uint8_t ioapic_next_irq_base = 16;

// First vector after last IOAPIC
static uint8_t ioapic_msi_base_intr;
static uint8_t ioapic_msi_base_irq;

static std::vector<uint8_t> isa_irq_lookup;

static unsigned apic_id_count;
static uint32_t apic_id_list[64];

static int16_t intr_to_irq[256];
static int16_t irq_to_intr[256];
static int16_t intr_to_ioapic[256];

static uint8_t topo_thread_bits;
static uint8_t topo_thread_count;
static uint8_t topo_core_bits;
static uint8_t topo_core_count;

static uint8_t topo_cpu_count;

static uintptr_t apic_base;
static uint32_t volatile *apic_ptr;

//
// APIC

// APIC ID (read only)
#define APIC_REG_ID                 0x02

// APIC version (read only)
#define APIC_REG_VER                0x03

// Task Priority Register
#define APIC_REG_TPR                0x08

// Arbitration Priority Register (not present in x2APIC mode)
#define APIC_REG_APR                0x09

// Processor Priority Register (read only)
#define APIC_REG_PPR                0x0A

// End Of Interrupt register (must write 0 in x2APIC mode)
#define APIC_REG_EOI                0x0B

// Logical Destination Register (not writeable in x2APIC mode)
#define APIC_REG_LDR                0x0D

// Destination Format Register (not present in x2APIC mode)
#define APIC_REG_DFR                0x0E

// Spurious Interrupt Register
#define APIC_REG_SIR                0x0F

// In Service Registers (bit n) (read only)
#define APIC_REG_ISR_n(n)           (0x10 + ((n) >> 5))

// Trigger Mode Registers (bit n) (read only)
#define APIC_REG_TMR_n(n)           (0x18 + ((n) >> 5))

// Interrupt Request Registers (bit n) (read only)
#define APIC_REG_IRR_n(n)           (0x20 + ((n) >> 5))

// Error Status Register (not present in x2APIC mode)
#define APIC_REG_ESR                0x28

// Local Vector Table Corrected Machine Check Interrupt
#define APIC_REG_LVT_CMCI           0x2F

// Local Vector Table Interrupt Command Register Lo (64-bit in x2APIC mode)
#define APIC_REG_ICR_LO             0x30

// Local Vector Table Interrupt Command Register Hi (not present in x2APIC mode)
#define APIC_REG_ICR_HI             0x31

// Local Vector Table Timer Register
#define APIC_REG_LVT_TR             0x32

// Local Vector Table Thermal Sensor Register
#define APIC_REG_LVT_TSR            0x33

// Local Vector Table Performance Monitoring Counter Register
#define APIC_REG_LVT_PMCR           0x34

// Local Vector Table Local Interrupt 0 Register
#define APIC_REG_LVT_LNT0           0x35

// Local Vector Table Local Interrupt 1 Register
#define APIC_REG_LVT_LNT1           0x36

// Local Vector Table Error Register
#define APIC_REG_LVT_ERR            0x37

// Local Vector Table Timer Initial Count Register
#define APIC_REG_LVT_ICR            0x38

// Local Vector Table Timer Current Count Register (read only)
#define APIC_REG_LVT_CCR            0x39

// Local Vector Table Timer Divide Configuration Register
#define APIC_REG_LVT_DCR            0x3E

// Self Interprocessor Interrupt (x2APIC only, write only)
#define APIC_REG_SELF_IPI           0x3F

#define APIC_CMD_DEST_MODE_PHYSICAL APIC_CMD_DEST_MODE_n(0)
#define APIC_CMD_DEST_MODE_LOGICAL  APIC_CMD_DEST_MODE_n(1)

#define APIC_CMD_DELIVERY_NORMAL    APIC_CMD_DELIVERY_n(0)
#define APIC_CMD_DELIVERY_LOWPRI    APIC_CMD_DELIVERY_n(1)
#define APIC_CMD_DELIVERY_SMI       APIC_CMD_DELIVERY_n(2)
#define APIC_CMD_DELIVERY_NMI       APIC_CMD_DELIVERY_n(4)
#define APIC_CMD_DELIVERY_INIT      APIC_CMD_DELIVERY_n(5)
#define APIC_CMD_DELIVERY_SIPI      APIC_CMD_DELIVERY_n(6)

#define APIC_CMD_DEST_TYPE_BYID     APIC_CMD_DEST_TYPE_n(0)
#define APIC_CMD_DEST_TYPE_SELF     APIC_CMD_DEST_TYPE_n(1)
#define APIC_CMD_DEST_TYPE_ALL      APIC_CMD_DEST_TYPE_n(2)
#define APIC_CMD_DEST_TYPE_OTHER    APIC_CMD_DEST_TYPE_n(3)

// Divide configuration register
#define APIC_LVT_DCR_BY_2           0
#define APIC_LVT_DCR_BY_4           1
#define APIC_LVT_DCR_BY_8           2
#define APIC_LVT_DCR_BY_16          3
#define APIC_LVT_DCR_BY_32          (8+0)
#define APIC_LVT_DCR_BY_64          (8+1)
#define APIC_LVT_DCR_BY_128         (8+2)
#define APIC_LVT_DCR_BY_1           (8+3)

#define APIC_LVT_TR_MODE_ONESHOT    0
#define APIC_LVT_TR_MODE_PERIODIC   1
#define APIC_LVT_TR_MODE_DEADLINE   2

#define APIC_LVT_DELIVERY_FIXED     0
#define APIC_LVT_DELIVERY_SMI       2
#define APIC_LVT_DELIVERY_NMI       4
#define APIC_LVT_DELIVERY_EXINT     7
#define APIC_LVT_DELIVERY_INIT      5

class lapic_t {
public:
    virtual void command(uint32_t dest, uint32_t cmd) const = 0;

    virtual uint32_t read32(uint32_t offset) const = 0;
    virtual void write32(uint32_t offset, uint32_t val) const = 0;

    virtual uint64_t read64(uint32_t offset) const = 0;
    virtual void write64(uint32_t offset, uint64_t val) const = 0;

    virtual bool reg_readable(uint32_t reg) const = 0;
    virtual bool reg_exists(uint32_t reg) const = 0;

protected:
    static bool reg_maybe_exists(uint32_t reg)
    {
        // Reserved registers:
        return  !(reg >= 0x40) &&
                !(reg <= 0x01) &&
                !(reg >= 0x04 && reg <= 0x07) &&
                !(reg == 0x0C) &&
                !(reg >= 0x29 && reg <= 0x2E) &&
                !(reg >= 0x3A && reg <= 0x3D) &&
                !(reg == 0x3F);
    }

    static bool reg_maybe_readable(uint32_t reg)
    {
        return reg != APIC_REG_EOI &&
                reg != APIC_REG_SELF_IPI &&
                reg_maybe_exists(reg);
    }
};

class lapic_x_t : public lapic_t {
    void command(uint32_t dest, uint32_t cmd) const override final
    {
        cpu_scoped_irq_disable intr_enabled;
        write32(APIC_REG_ICR_HI, APIC_DEST_n(dest));
        write32(APIC_REG_ICR_LO, cmd);
        intr_enabled.restore();
        while (read32(APIC_REG_ICR_LO) & APIC_CMD_PENDING)
            pause();
    }

    uint32_t read32(uint32_t offset) const override final
    {
        return apic_ptr[offset << (4 - 2)];
    }

    void write32(uint32_t offset, uint32_t val) const override final
    {
        apic_ptr[offset << (4 - 2)] = val;
    }

    uint64_t read64(uint32_t offset) const override final
    {
        return ((uint64_t*)apic_ptr)[offset << (4 - 3)];
    }

    void write64(uint32_t offset, uint64_t val) const override final
    {
        ((uint64_t*)apic_ptr)[offset << (4 - 3)] = val;
    }

    bool reg_readable(uint32_t reg) const override final
    {
        return reg_maybe_readable(reg);
    }

    bool reg_exists(uint32_t reg) const override final
    {
        return reg_maybe_readable(reg);
    }
};

class lapic_x2_t : public lapic_t {
    void command(uint32_t dest, uint32_t cmd) const override final
    {
        write64(APIC_REG_ICR_LO, (uint64_t(dest) << 32) | cmd);
    }

    uint32_t read32(uint32_t offset) const override final
    {
        return cpu_msr_get_lo(0x800 + offset);
    }

    void write32(uint32_t offset, uint32_t val) const override final
    {
        cpu_msr_set(0x800 + offset, val);
    }

    uint64_t read64(uint32_t offset) const override final
    {
        return cpu_msr_get(0x800 + offset);
    }

    void write64(uint32_t offset, uint64_t val) const override final
    {
        cpu_msr_set(0x800 + offset, val);
    }

    bool reg_readable(uint32_t reg) const override final
    {
        // APIC_REG_LVT_CMCI raises #GP if CMCI not enabled
        return reg != APIC_REG_LVT_CMCI &&
                reg != APIC_REG_ICR_HI &&
                reg_exists(reg) &&
                reg_maybe_readable(reg);
    }

    bool reg_exists(uint32_t reg) const override final
    {
        return reg != APIC_REG_DFR &&
                reg != APIC_REG_APR &&
                reg_maybe_exists(reg);
    }
};

static lapic_x_t apic_x;
static lapic_x2_t apic_x2;
static lapic_t *apic;

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

static std::vector<acpi_gas_t> acpi_hpet_list;
static int acpi_madt_flags;

// Local interrupt NMI LINT MADT records
static std::vector<acpi_madt_lnmi_t> lapic_lint_nmi;

static acpi_fadt_t acpi_fadt;

// The ACPI PM timer runs at 3.579545MHz
#define ACPI_PM_TIMER_HZ    3579545

static uint64_t acpi_rsdt_addr;
static uint64_t acpi_rsdt_len;
static uint8_t acpi_rsdt_ptrsz;

int acpi_have8259pic(void)
{
    return !acpi_rsdt_addr ||
            !!(acpi_madt_flags & ACPI_MADT_FLAGS_HAVE_PIC);
}

static uint8_t ioapic_alloc_vectors(uint8_t count)
{
    assert(count <= 64);

    if (count > 64)
        return 0;

    uint8_t result = 0;

    ioapic_msi_alloc_scoped_lock lock(ioapic_msi_alloc_lock);

    for (size_t bit = 0; bit < INTR_APIC_IRQ_COUNT; )
    {
        // Skip completely allocated chunk of 64 vectors
        if ((bit & 63) == 0) {
            size_t i = bit >> 6;
            if (ioapic_msi_alloc_map[i] == (uint64_t)-1) {
                bit += 64;
                continue;
            }
        }

        // Make sure a consecutive range of bits are all unallocated
        size_t ofs;
        for (ofs = 0; ofs < count; ++ofs) {
            size_t i = (bit + ofs) >> 6;
            uint64_t chk = UINT64_C(1) << ((bit + ofs) & 63);

            if (ioapic_msi_alloc_map[i] & chk)
                break;
        }

        if (ofs < count) {
            bit += ofs + 1;
            continue;
        }

        for (ofs = 0; ofs < count; ++ofs) {
            size_t i = (bit + ofs) >> 6;
            uint64_t chk = UINT64_C(1) << ((bit + ofs) & 63);

            ioapic_msi_alloc_map[i] |= chk;
        }

        result = bit + INTR_APIC_IRQ_BASE;

        return result;
    }

    return INTR_APIC_IRQ_END - count;
}

// Returns 0 on failure
// Pass 0 to allocate 1 vector, 1 to allocate 2 vectors,
// 4 to allocate 16 vectors, etc.
// Returns a vector with log2n LSB bits clear.
static uint8_t ioapic_aligned_vectors(uint8_t log2n)
{
    int count = 1 << log2n;

    uint64_t mask = ~((uint64_t)-1 << count);
    uint64_t checked = mask;
    uint8_t result = 0;

    ioapic_msi_alloc_scoped_lock lock(ioapic_msi_alloc_lock);

    for (size_t bit = 0; bit < INTR_APIC_IRQ_COUNT; bit += count)
    {
        size_t i = bit >> 6;

        if (!(ioapic_msi_alloc_map[i] & checked)) {
            ioapic_msi_alloc_map[i] |= checked;
            result = bit + INTR_APIC_IRQ_BASE;
            break;
        }
        checked <<= count;
        if (!checked)
            checked = mask;
    }

    return result;
}

static _always_inline uint8_t checksum_bytes(char const *bytes, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
        sum += (uint8_t)bytes[i];
    return sum;
}

static void acpi_process_fadt(acpi_fadt_t *fadt_hdr)
{
    acpi_fadt = *fadt_hdr;


}

template<typename H, typename E, typename F>
static bool acpi_foreach_record(H *hdr, E *ent_type, F callback, int type)
{
    E *ent = (E*)(hdr + 1);
    E *end = (E*)(uintptr_t(hdr) + hdr->hdr.len);

    for ( ; ent < end; ent = (E*)(uintptr_t(ent) + ent->hdr.record_len)) {
        if (ent->hdr.entry_type == type || type == -1)
            callback(ent);
    }
    return true;
}

static void acpi_process_madt(acpi_madt_t *madt_hdr)
{
    acpi_madt_ent_t *ent = (acpi_madt_ent_t*)(madt_hdr + 1);
    acpi_madt_ent_t *end = (acpi_madt_ent_t*)
            ((char*)madt_hdr + madt_hdr->hdr.len);

    apic_base = madt_hdr->lapic_address;
    acpi_madt_flags = madt_hdr->flags & 1;

    // Scan for APIC ID records
    acpi_foreach_record(madt_hdr, (acpi_madt_ent_t*)nullptr,
                        [](acpi_madt_ent_t* ent) {
        if (apic_id_count < countof(apic_id_list)) {
            ACPI_TRACE("Found LAPIC, ID=%d\n", ent->lapic.apic_id);

            // If processor is enabled
            if (ent->lapic.flags == 1) {
                apic_id_list[apic_id_count++] = ent->lapic.apic_id;
            } else {
                ACPI_TRACE("Disabled processor detected\n");
            }
        } else {
            ACPI_ERROR("Too many CPUs! Dropped one\n");
        }
    }, ACPI_MADT_REC_TYPE_LAPIC);

    // Scan for IOAPIC records
    acpi_foreach_record(madt_hdr, (acpi_madt_ent_t*)nullptr,
                        [](acpi_madt_ent_t* ent) {
        ACPI_TRACE("IOAPIC found\n");
        if (ioapic_count < countof(ioapic_list)) {
            mp_ioapic_t *ioapic = ioapic_list + ioapic_count++;
            ioapic->addr = ent->ioapic.addr;
            ioapic->irq_base = ent->ioapic.irq_base;
            ioapic->id = ent->ioapic.apic_id;
            ioapic->ptr = (uint32_t*)mmap(
                        (void*)(uintptr_t)ent->ioapic.addr, 12,
                        PROT_READ | PROT_WRITE,
                        MAP_PHYSICAL | MAP_NOCACHE |
                        MAP_WRITETHRU, -1, 0);

            ioapic->ptr[IOAPIC_IOREGSEL] = IOAPIC_REG_VER;
            uint32_t entries = ioapic->ptr[IOAPIC_IOREGWIN];
            entries = IOAPIC_VER_ENTRIES_GET(entries) + 1;

            ioapic->vector_count = entries;
            ioapic->base_intr = ioapic_alloc_vectors(entries);

            ioapic_reset(ioapic);

            ACPI_TRACE("IOAPIC registered, id=%u, addr=%#x, irqbase=%d,"
                       " entries=%u\n",
                       ioapic->id, ioapic->addr, ioapic->irq_base,
                       ioapic->vector_count);
        } else {
            ACPI_TRACE("Too many IOAPICs!\n");
        }
    }, ACPI_MADT_REC_TYPE_IOAPIC);

    for ( ; ent < end;
          ent = (acpi_madt_ent_t*)((char*)ent + ent->hdr.record_len)) {
        ACPI_TRACE("FADT record, type=%#x ptr=%p, len=%u\n",
                   ent->hdr.entry_type, (void*)ent,
                   ent->ioapic.hdr.record_len);
        switch (ent->hdr.entry_type) {
        case ACPI_MADT_REC_TYPE_LAPIC:
            break;

        case ACPI_MADT_REC_TYPE_IOAPIC:
            break;

        case ACPI_MADT_REC_TYPE_IRQ:
        {
            ACPI_TRACE("Got Interrupt redirection table\n");

            uint8_t gsi = ent->irq_src.gsi;
            uint8_t intr = INTR_APIC_IRQ_BASE + gsi;
            uint8_t irq = ent->irq_src.irq_src;
            uint8_t bus = ent->irq_src.bus;
            bool isa = (bus == 0);

            intr_to_irq[intr] = irq;
            irq_to_intr[irq] = intr;

            int ioapic_index = intr_to_ioapic[intr];

            ACPI_TRACE("Applied redirection, irq=%u, gsi=%u"
                       ", flags=%x vector=%u, ioapic_index=%d\n",
                       irq, gsi, ent->irq_src.flags, intr, ioapic_index);

            mp_ioapic_t *ioapic = ioapic_list + ioapic_index;
            ioapic_set_flags(ioapic, irq - ioapic->irq_base,
                             ent->irq_src.flags, isa);

            break;
        }

        case ACPI_MADT_REC_TYPE_NMI:
        {
            ACPI_TRACE("Got IOAPIC NMI mapping\n");

            uint8_t gsi = ent->nmi_src.gsi;
            uint8_t intr = INTR_APIC_IRQ_BASE + gsi;
            int ioapic_index = intr_to_ioapic[intr];
            if (ioapic_index < 0) {
                ACPI_TRACE("Got IOAPIC NMI mapping"
                           " but failed to lookup IOAPIC\n");
                break;
            }
            mp_ioapic_t *ioapic = ioapic_list + ioapic_index;
            // flags is delivery type?
            ioapic_set_type(ioapic, gsi - ioapic->irq_base, ent->nmi_src.flags);
            break;
        }

        case ACPI_MADT_REC_TYPE_LNMI:
        {
            ACPI_TRACE("Got IOAPIC LNMI mapping\n");

            lapic_lint_nmi.push_back(ent->lnmi);

            break;
        }

        case ACPI_MADT_REC_TYPE_X2APIC:
        {
            ACPI_TRACE("Got X2APIC\n");

            if (apic_id_count < countof(apic_id_list)) {
                if (ent->lapic.flags == 1)
                    apic_id_list[apic_id_count++] = ent->x2apic.x2apic_id;
            } else {
                ACPI_ERROR("Too many CPUs! Dropped one\n");
            }
            break;
        }

        default:
            ACPI_TRACE("Unrecognized FADT record, entry_type=%#.2x\n",
                       ent->hdr.entry_type);
            break;

        }
    }
}

static void acpi_process_hpet(acpi_hpet_t *acpi_hdr)
{
    acpi_hpet_list.push_back(acpi_hdr->addr);
}

static _always_inline uint8_t acpi_chk_hdr(acpi_sdt_hdr_t *hdr)
{
    return checksum_bytes((char const *)hdr, hdr->len);
}

// Sometimes we have to guess the size
// then read the header to get the actual size.
// This handles that.
template<typename T>
static T *acpi_remap_len(T *ptr, uintptr_t physaddr,
                         size_t guess, size_t actual_len)
{
    if (actual_len > guess) {
        munmap(ptr, guess);
        ptr = (T*)mmap((void*)physaddr, actual_len,
                       PROT_READ, MAP_PHYSICAL, -1, 0);
    }

    return ptr;
}

static void acpi_parse_rsdt()
{
    // Sanity check length (<= 1MB)
    assert(acpi_rsdt_len <= (1 << 20));

    acpi_sdt_hdr_t *rsdt_hdr = (acpi_sdt_hdr_t *)mmap(
                (void*)acpi_rsdt_addr,
                acpi_rsdt_len ? acpi_rsdt_len : sizeof(*rsdt_hdr),
                PROT_READ, MAP_PHYSICAL, -1, 0);

    // For ACPI 1.0, get length from header and remap
    if (!acpi_rsdt_len) {
        acpi_rsdt_len = rsdt_hdr->len;
        rsdt_hdr = acpi_remap_len(rsdt_hdr, acpi_rsdt_addr,
                                  sizeof(*rsdt_hdr), acpi_rsdt_len);
    }

    ACPI_TRACE("RSDT version %#x\n", rsdt_hdr->rev);

    if (acpi_chk_hdr(rsdt_hdr) != 0) {
        ACPI_ERROR("ACPI RSDT checksum mismatch!\n");
        return;
    }

    uint32_t *rsdp_ptrs = (uint32_t *)(rsdt_hdr + 1);
    uint32_t *rsdp_end = (uint32_t *)((char*)rsdt_hdr + rsdt_hdr->len);

    ACPI_TRACE("Processing RSDP pointers from %p to %p\n",
           (void*)rsdp_ptrs, (void*)rsdp_end);

    for (uint32_t *rsdp_ptr = rsdp_ptrs;
         rsdp_ptr < rsdp_end; rsdp_ptr += (acpi_rsdt_ptrsz >> 2)) {
        uint64_t hdr_addr;

        if (acpi_rsdt_ptrsz == sizeof(uint32_t))
            hdr_addr = *rsdp_ptr;
        else
            hdr_addr = *(uint64_t*)rsdp_ptr;

        acpi_sdt_hdr_t *hdr = (acpi_sdt_hdr_t *)
                mmap((void*)hdr_addr,
                      64 << 10, PROT_READ, MAP_PHYSICAL, -1, 0);

        hdr = acpi_remap_len(hdr, hdr_addr, 64 << 10, hdr->len);

        if (!memcmp(hdr->sig, "FACP", 4)) {
            acpi_fadt_t *fadt_hdr = (acpi_fadt_t *)hdr;

            if (acpi_chk_hdr(&fadt_hdr->hdr) == 0) {
                ACPI_TRACE("ACPI FADT found\n");
                acpi_process_fadt(fadt_hdr);
            } else {
                ACPI_ERROR("ACPI FADT checksum mismatch!\n");
            }
        } else if (!memcmp(hdr->sig, "APIC", 4)) {
            acpi_madt_t *madt_hdr = (acpi_madt_t *)hdr;

            if (acpi_chk_hdr(&madt_hdr->hdr) == 0) {
                ACPI_TRACE("ACPI MADT found\n");
                acpi_process_madt(madt_hdr);
            } else {
                ACPI_ERROR("ACPI MADT checksum mismatch!\n");
            }
        } else if (!memcmp(hdr->sig, "HPET", 4)) {
            acpi_hpet_t *hpet_hdr = (acpi_hpet_t *)hdr;

            if (acpi_chk_hdr(&hpet_hdr->hdr) == 0) {
                ACPI_TRACE("ACPI HPET found\n");
                acpi_process_hpet(hpet_hdr);
            } else {
                ACPI_ERROR("ACPI MADT checksum mismatch!\n");
            }
        } else if (!memcmp(hdr->sig, "MCFG", 4)) {
            acpi_mcfg_hdr_t *mcfg_hdr = (acpi_mcfg_hdr_t*)hdr;

            if (acpi_chk_hdr(&mcfg_hdr->hdr) == 0) {
                acpi_ecam_rec_t *ecam_ptr = (acpi_ecam_rec_t*)(mcfg_hdr+1);
                acpi_ecam_rec_t *ecam_end = (acpi_ecam_rec_t*)
                        ((char*)mcfg_hdr + mcfg_hdr->hdr.len);
                size_t ecam_count = ecam_end - ecam_ptr;
                pci_init_ecam(ecam_count);
                for (size_t i = 0; i < ecam_count; ++i) {
                    ACPI_TRACE("PCIe ECAM ptr=%#" PRIx64 " busses=%u-%u\n",
                               ecam_ptr[i].ecam_base,
                               ecam_ptr[i].st_bus, ecam_ptr[i].en_bus);
                    pci_init_ecam_entry(ecam_ptr[i].ecam_base,
                                        ecam_ptr[i].segment_group,
                                        ecam_ptr[i].st_bus,
                                        ecam_ptr[i].en_bus);
                }
                pci_init_ecam_enable();
            }
        } else if (!memcmp(hdr->sig, "SRAT", 4)) {
            acpi_srat_hdr_t *srat_hdr = (acpi_srat_hdr_t *)hdr;

            if (acpi_chk_hdr(&srat_hdr->hdr) == 0) {
                ACPI_TRACE("SRAT found\n");

                acpi_srat_rec_hdr_t *srat_end = (acpi_srat_rec_hdr_t *)
                        ((char*)srat_hdr + srat_hdr->hdr.len);

                for (auto rec_hdr = (acpi_srat_rec_hdr_t*)(srat_hdr+1);
                     rec_hdr < srat_end;
                     rec_hdr = (acpi_srat_rec_hdr_t*)
                     ((char*)rec_hdr + rec_hdr->len)) {
                    acpi_srat_lapic_t *lapic_rec;
                    acpi_srat_mem_t *mem_rec;
                    acpi_srat_x2apic_t *x2apic_rec;
                    switch (rec_hdr->type) {
                    case 0:
                        // LAPIC affinity
                        lapic_rec = (acpi_srat_lapic_t*)rec_hdr;
                        ACPI_TRACE("Got LAPIC affinity record"
                                   ", domain=%#x"
                                   ", apic_id=%#x"
                                   ", enabled=%u"
                                   "\n",
                                   lapic_rec->domain_lo |
                                   (lapic_rec->domain_hi[0] << 8) |
                                   (lapic_rec->domain_hi[1] << 16) |
                                   (lapic_rec->domain_hi[2] << 24),
                                   lapic_rec->apic_id,
                                   lapic_rec->flags);
                        break;

                    case 1:
                        // Memory affinity
                        mem_rec = (acpi_srat_mem_t*)rec_hdr;
                        ACPI_TRACE("Got memory affinity record"
                                   ", domain=%#x"
                                   ", enabled=%u"
                                   ", base=%#" PRIx64
                                   ", len=%#" PRIx64
                                   "\n",
                                   mem_rec->domain,
                                   mem_rec->flags,
                                   mem_rec->range_base,
                                   mem_rec->range_length);
                        break;

                    case 2:
                        // x2APIC affinity
                        x2apic_rec = (acpi_srat_x2apic_t*)rec_hdr;
                        ACPI_TRACE("Got x2APIC affinity record"
                                   ", domain=%#x"
                                   ", apic_id=%#x"
                                   ", enabled=%u"
                                   "\n",
                                   x2apic_rec->domain,
                                   x2apic_rec->x2apic_id,
                                   x2apic_rec->flags);
                        break;

                    default:
                        ACPI_TRACE("Got unrecognized affinity record\n");
                        break;

                    }
                }
            }
        } else {
            if (acpi_chk_hdr(hdr) == 0) {
                ACPI_TRACE("ACPI %4.4s ignored\n", hdr->sig);
            } else {
                ACPI_ERROR("ACPI %4.4s checksum mismatch!"
                       " (ignored anyway)\n", hdr->sig);
            }
        }

        munmap(hdr, std::max(size_t(64 << 10), size_t(hdr->len)));
    }
}

static void mp_parse_fps()
{
    mp_cfg_tbl_hdr_t *cth = (mp_cfg_tbl_hdr_t *)
            mmap((void*)mp_tables, 0x10000,
                 PROT_READ, MAP_PHYSICAL, -1, 0);

    cth = acpi_remap_len(cth, uintptr_t(mp_tables),
                         0x10000, cth->base_tbl_len + cth->ext_tbl_len);

    uint8_t *entry = (uint8_t*)(cth + 1);

    // Reset to impossible values
    mp_isa_bus_id = -1;

    // First slot reserved for BSP
    apic_id_count = 1;

    std::fill_n(intr_to_irq, countof(intr_to_irq), -1);
    std::fill_n(irq_to_intr, countof(irq_to_intr), -1);
    std::fill_n(intr_to_ioapic, countof(intr_to_ioapic), -1);

    for (uint16_t i = 0; i < cth->entry_count; ++i) {
        mp_cfg_cpu_t *entry_cpu;
        mp_cfg_bus_t *entry_bus;
        mp_cfg_ioapic_t *entry_ioapic;
        mp_cfg_iointr_t *entry_iointr;
        mp_cfg_lintr_t *entry_lintr;
        mp_cfg_addrmap_t *entry_addrmap;
        mp_cfg_bushier_t *entry_busheir;
        mp_cfg_buscompat_t *entry_buscompat;
        switch (*entry) {
        case MP_TABLE_TYPE_CPU:
        {
            entry_cpu = (mp_cfg_cpu_t *)entry;

            MPS_TRACE("CPU package found,"
                      " base apic id=%u ver=%#x\n",
                      entry_cpu->apic_id, entry_cpu->apic_ver);

            if ((entry_cpu->flags & MP_CPU_FLAGS_ENABLED) &&
                    apic_id_count < countof(apic_id_list)) {
                if (entry_cpu->flags & MP_CPU_FLAGS_BSP)
                    apic_id_list[0] = entry_cpu->apic_id;
                else
                    apic_id_list[apic_id_count++] = entry_cpu->apic_id;
            }

            entry = (uint8_t*)(entry_cpu + 1);
            break;
        }

        case MP_TABLE_TYPE_BUS:
        {
            entry_bus = (mp_cfg_bus_t *)entry;

            MPS_TRACE("%.*s bus found, id=%u\n",
                     (int)sizeof(entry_bus->type),
                     entry_bus->type, entry_bus->bus_id);

            if (!memcmp(entry_bus->type, "PCI   ", 6)) {
                mp_pci_bus_ids.push_back(entry_bus->bus_id);
            } else if (!memcmp(entry_bus->type, "ISA   ", 6)) {
                if (mp_isa_bus_id == 0xFFFF)
                    mp_isa_bus_id = entry_bus->bus_id;
                else
                    MPS_ERROR("Too many ISA busses,"
                              " only one supported\n");
            } else {
                MPS_ERROR("Dropped! Unrecognized bus named \"%.*s\"\n",
                          (int)sizeof(entry_bus->type), entry_bus->type);
            }
            entry = (uint8_t*)(entry_bus + 1);
            break;
        }

        case MP_TABLE_TYPE_IOAPIC:
        {
            entry_ioapic = (mp_cfg_ioapic_t *)entry;

            if (entry_ioapic->flags & MP_IOAPIC_FLAGS_ENABLED) {
                if (ioapic_count >= countof(ioapic_list)) {
                    MPS_ERROR("Dropped! Too many IOAPIC devices\n");
                    break;
                }

                MPS_TRACE("IOAPIC id=%d, addr=%#x,"
                          " flags=%#x, ver=%#x\n",
                          entry_ioapic->id,
                          entry_ioapic->addr,
                          entry_ioapic->flags,
                          entry_ioapic->ver);

                mp_ioapic_t *ioapic = ioapic_list + ioapic_count++;

                ioapic->id = entry_ioapic->id;
                ioapic->addr = entry_ioapic->addr;

                uint32_t volatile *ioapic_ptr = (uint32_t *)mmap(
                            (void*)(uintptr_t)entry_ioapic->addr,
                            12, PROT_READ | PROT_WRITE,
                            MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU, -1, 0);

                ioapic->ptr = ioapic_ptr;

                // Read redirection table size

                ioapic_ptr[IOAPIC_IOREGSEL] = IOAPIC_REG_VER;
                uint32_t ioapic_ver = ioapic_ptr[IOAPIC_IOREGWIN];

                uint8_t ioapic_intr_count =
                        IOAPIC_VER_ENTRIES_GET(ioapic_ver) + 1;

                // Allocate virtual IRQ numbers
                ioapic->irq_base = ioapic_next_irq_base;
                ioapic_next_irq_base += ioapic_intr_count;

                // Allocate vectors, assign range to IOAPIC
                ioapic->vector_count = ioapic_intr_count;
                ioapic->base_intr = ioapic_alloc_vectors(ioapic_intr_count);

                ioapic_reset(ioapic);
            }
            entry = (uint8_t*)(entry_ioapic + 1);
            break;
        }

        case MP_TABLE_TYPE_IOINTR:
        {
            entry_iointr = (mp_cfg_iointr_t *)entry;

            uint16_t intr_flags = entry_iointr->flags;
            uint8_t ioapic_id = entry_iointr->dest_ioapic_id;
            uint8_t intin = entry_iointr->dest_ioapic_intin;
            uint8_t bus = entry_iointr->source_bus;
            bool isa = (bus == 0);

            ioapic_set_flags(ioapic_by_id(ioapic_id), intin, intr_flags, isa);

            entry = (uint8_t*)(entry_iointr + 1);
            break;
        }

        case MP_TABLE_TYPE_LINTR:
        {
            entry_lintr = (mp_cfg_lintr_t*)entry;
            if (memchr(mp_pci_bus_ids.data(), entry_lintr->source_bus,
                        mp_pci_bus_ids.size())) {
                uint8_t device = entry_lintr->source_bus_irq >> 2;
                uint8_t pci_irq = entry_lintr->source_bus_irq;
                uint8_t lapic_id = entry_lintr->dest_lapic_id;
                uint8_t intin = entry_lintr->dest_lapic_lintin;
                MPS_TRACE("PCI device %u INT_%c# ->"
                          " LAPIC ID %#.2x INTIN %d\n",
                          device, (pci_irq & 3) + 'A',
                          lapic_id, intin);
            } else if (entry_lintr->source_bus == mp_isa_bus_id) {
                uint8_t isa_irq = entry_lintr->source_bus_irq;
                uint8_t lapic_id = entry_lintr->dest_lapic_id;
                uint8_t intin = entry_lintr->dest_lapic_lintin;

                MPS_TRACE("ISA IRQ %d -> LAPIC ID %#.2x INTIN %u\n",
                          isa_irq, lapic_id, intin);
            } else {
                // Unknown bus!
                MPS_ERROR("IRQ %d on unknown bus ->"
                          " IOAPIC ID %#.2x INTIN %u\n",
                          entry_lintr->source_bus_irq,
                          entry_lintr->dest_lapic_id,
                          entry_lintr->dest_lapic_lintin);
            }
            entry = (uint8_t*)(entry_lintr + 1);
            break;
        }

        case MP_TABLE_TYPE_ADDRMAP:
        {
            entry_addrmap = (mp_cfg_addrmap_t*)entry;
            uint8_t bus = entry_addrmap->bus_id;
            uint64_t addr =  entry_addrmap->addr_lo |
                    ((uint64_t)entry_addrmap->addr_hi << 32);
            uint64_t len =  entry_addrmap->addr_lo |
                    ((uint64_t)entry_addrmap->addr_hi << 32);

            MPS_TRACE("Address map, bus=%d, addr=%#" PRIx64
                      ", len=%#" PRIx64 "\n",
                      bus, addr, len);

            entry += entry_addrmap->len;
            break;
        }

        case MP_TABLE_TYPE_BUSHIER:
        {
            entry_busheir = (mp_cfg_bushier_t*)entry;
            uint8_t bus = entry_busheir->bus_id;
            uint8_t parent_bus = entry_busheir->parent_bus;
            uint8_t info = entry_busheir->info;

            MPS_TRACE("Bus hierarchy, bus=%d, parent=%d, info=%x\n",
                      bus, parent_bus, info);

            entry += entry_busheir->len;
            break;
        }

        case MP_TABLE_TYPE_BUSCOMPAT:
        {
            entry_buscompat = (mp_cfg_buscompat_t*)entry;
            uint8_t bus = entry_buscompat->bus_id;
            uint8_t bus_mod = entry_buscompat->bus_mod;
            uint32_t bus_predef = entry_buscompat->predef_range_list;

            MPS_TRACE("Bus compat, bus=%d, mod=%d,"
                      " predefined_range_list=%x\n",
                      bus, bus_mod, bus_predef);

            entry += entry_buscompat->len;
            break;
        }

        default:
            MPS_ERROR("Unknown MP table entry_type!"
                      " Guessing size is 8\n");
            // Hope for the best here
            entry += 8;
            break;
        }
    }

    munmap(cth, std::max(0x10000, cth->base_tbl_len + cth->ext_tbl_len));
}

static int parse_mp_tables(void)
{
    boottbl_acpi_info_t const *acpi_info;
    acpi_info = (boottbl_acpi_info_t const *)bootinfo_parameter(
                bootparam_t::boot_acpi_rsdp);

    if (acpi_info) {
        acpi_rsdt_addr = acpi_info->rsdt_addr;
        acpi_rsdt_len = acpi_info->rsdt_size;
        acpi_rsdt_ptrsz = acpi_info->ptrsz;
    }

    boottbl_mptables_info_t *mps_info;
    mps_info = (boottbl_mptables_info_t *)bootinfo_parameter(
                bootparam_t::boot_mptables);

    if (mps_info) {
        mp_tables = (char const *)mps_info->mp_addr;
    }

    if (acpi_rsdt_addr)
        acpi_parse_rsdt();
    else if (mp_tables)
        mp_parse_fps();

    return !!mp_tables;
}

static isr_context_t *apic_timer_handler(int intr, isr_context_t *ctx)
{
    apic_eoi(intr);
    return thread_schedule(ctx);
}

static isr_context_t *apic_spurious_handler(int intr, isr_context_t *ctx)
{
    (void)intr;
    assert(intr == INTR_APIC_SPURIOUS);
    APIC_ERROR("Spurious APIC interrupt!\n");
    cpu_debug_break();
    return ctx;
}

static isr_context_t *apic_err_handler(int intr, isr_context_t *ctx)
{
    (void)intr;
    assert(intr == INTR_APIC_ERROR);
    APIC_ERROR("APIC error interrupt!\n");
    return ctx;
}

unsigned apic_get_id(void)
{
    if (likely(apic))
        return apic->read32(APIC_REG_ID);

    cpuid_t cpuid_info;
    cpuid(&cpuid_info, CPUID_INFO_FEATURES, 0);
    unsigned apic_id = cpuid_info.ebx >> 24;
    return apic_id;
}

static void apic_send_command(uint32_t dest, uint32_t cmd)
{
    apic->command(dest, cmd);
}

void apic_send_ipi(int target_apic_id, uint8_t intr)
{
    if (unlikely(!apic))
        return;

    uint32_t dest_type = (target_apic_id < -1)
            ? APIC_CMD_DEST_TYPE_ALL
            : (target_apic_id < 0)
            ? APIC_CMD_DEST_TYPE_OTHER
            : APIC_CMD_DEST_TYPE_BYID;

    uint32_t dest_mode = (intr != INTR_EX_NMI)
            ? APIC_CMD_DELIVERY_NORMAL
            : APIC_CMD_DELIVERY_NMI;

    uint32_t dest = (target_apic_id >= 0) ? target_apic_id : 0;

    if (intr != INTR_TLB_SHOOTDOWN) {
        APIC_TRACE("IPI: intr=%x dest_type=%x dest_mode=%x cmd=%x\n",
                   intr, dest_type, dest_mode,
                   APIC_CMD_VECTOR_n(intr) | dest_type | dest_mode);
    }

    apic_send_command(dest, APIC_CMD_VECTOR_n(intr) | dest_type | dest_mode);
}

void apic_eoi(int intr)
{
    apic->write32(APIC_REG_EOI, intr & 0);
}

static void apic_online(int enabled, int spurious_intr, int err_intr)
{
    uint32_t sir = apic->read32(APIC_REG_SIR);

    if (enabled) {
        // Enable APIC, enable EOI broadcast, enable focus processor checking
        APIC_SIR_APIC_ENABLE_SET(sir, 1);
        APIC_SIR_NO_EOI_BCAST_SET(sir, 0);
        APIC_SIR_NO_FOCUS_CHK_SET(sir, 0);
    } else {
        // Disable APIC
        APIC_SIR_APIC_ENABLE_SET(sir, 0);
    }

    if (spurious_intr >= 32) {
        APIC_TRACE("spurious interrupt=%d\n", spurious_intr);
        APIC_SIR_VECTOR_SET(sir, spurious_intr);
    }

    // LDR is read only in x2APIC mode
    if (apic != &apic_x2)
        apic->write32(APIC_REG_LDR, 0xFFFFFFFF);

    apic->write32(APIC_REG_SIR, sir);

    apic->write32(APIC_REG_LVT_ERR,
                  APIC_LVT_VECTOR_n(err_intr) |
                  APIC_LVT_DELIVERY_n(0) |
                  APIC_LVT_MASK_n(0));

    apic->write32(APIC_REG_ESR, 0);
}

void apic_dump_regs(int ap)
{
#if DEBUG_APIC
    if (apic == nullptr) {
        printdbg("APIC not initialized\n");
        return;
    }

    for (int i = 0; i < 64; i += 4) {
        printdbg("ap=%d APIC: ", ap);
        for (int x = 0; x < 4; ++x) {
            if (apic->reg_readable(i + x)) {
                printdbg("%s[%3x]=%#.8x%s",
                         x == 0 ? "apic: " : "",
                         (i + x),
                         apic->read32(i + x),
                         x == 3 ? "\n" : " ");
            } else {
                printdbg("%s[%3x]=--------%s",
                         x == 0 ? "apic: " : "",
                         i + x,
                         x == 3 ? "\n" : " ");
            }
        }
    }
    APIC_TRACE("Logical destination register value: %#x\n",
             apic->read32(APIC_REG_LDR));
#endif
}

static void apic_calibrate();

static void apic_configure_timer(
        uint32_t dcr, uint32_t icr, uint8_t timer_mode,
        uint8_t intr, bool mask = false)
{
    APIC_TRACE("configuring timer,"
               " dcr=%#x, icr=%#x, mode=%#x, intr=%#x, mask=%d\n",
               dcr, icr, timer_mode, intr, mask);
    apic->write32(APIC_REG_LVT_DCR, dcr);
    apic->write32(APIC_REG_LVT_TR, APIC_LVT_VECTOR_n(intr) |
                  APIC_LVT_TR_MODE_n(timer_mode) |
                  (mask ? APIC_LVT_MASK : 0));
    apic->write32(APIC_REG_LVT_ICR, icr);
}

int apic_init(int ap)
{
    uint64_t apic_base_msr = cpu_msr_get(CPU_APIC_BASE_MSR);

    if (!apic_base)
        apic_base = apic_base_msr & CPU_APIC_BASE_ADDR;

    if (!(apic_base_msr & CPU_APIC_BASE_GENABLE)) {
        APIC_TRACE("APIC was globally disabled!"
                   " Enabling...\n");
    }

    if (cpuid_has_x2apic()) {
        APIC_TRACE("Using x2APIC\n");
        if (!ap)
            apic = &apic_x2;

        cpu_msr_set(CPU_APIC_BASE_MSR, apic_base_msr |
                CPU_APIC_BASE_GENABLE| CPU_APIC_BASE_X2ENABLE);
    } else {
        APIC_TRACE("Using xAPIC\n");

        if (!ap) {
            // Bootstrap CPU only
            assert(!apic_ptr);
            apic_ptr = (uint32_t *)mmap(
                        (void*)(apic_base),
                        4096, PROT_READ | PROT_WRITE,
                        MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU, -1, 0);

            apic = &apic_x;
        }

        cpu_msr_set(CPU_APIC_BASE_MSR,
                    (apic_base_msr & ~CPU_APIC_BASE_X2ENABLE) |
                    CPU_APIC_BASE_GENABLE);
    }

    // Set global enable if it is clear
    if (!(apic_base_msr & CPU_APIC_BASE_GENABLE)) {
        APIC_TRACE("APIC was globally disabled!"
                   " Enabling...\n");
    }

    if (!ap) {
        intr_hook(INTR_APIC_TIMER, apic_timer_handler, "apic_timer");
        intr_hook(INTR_APIC_SPURIOUS, apic_spurious_handler, "apic_spurious");
        intr_hook(INTR_APIC_ERROR, apic_err_handler, "apic_error");

        APIC_TRACE("Parsing boot tables\n");

        parse_mp_tables();

        APIC_TRACE("Calibrating APIC timer\n");

        apic_calibrate();
    }

    APIC_TRACE("Enabling APIC\n");

    apic_online(1, INTR_APIC_SPURIOUS, INTR_APIC_ERROR);

    apic->write32(APIC_REG_TPR, 0x0);

    assert(apic_base == (cpu_msr_get(CPU_APIC_BASE_MSR) & CPU_APIC_BASE_ADDR));

    if (ap) {
        APIC_TRACE("Configuring AP timer\n");
        apic_configure_timer(APIC_LVT_DCR_BY_1,
                             apic_timer_freq / 20,
                             APIC_LVT_TR_MODE_PERIODIC,
                             INTR_APIC_TIMER);
    }

    //apic_dump_regs(ap);

    return 1;
}

static void apic_detect_topology_amd(void)
{
    cpuid_t info;

    if (unlikely(!cpuid(&info, 1, 0)))
        return;

    topo_thread_count = (info.ebx >> 16) & 0xFF;

    if (cpuid(&info, 0x80000008, 0)) {
        // First check ECX bits 12 to 15,
        // if it is not zero, then it contains "core_bits".
        // Otherwise, use ECX bits 0 to 7 to determine the number of cores,
        // round it up to the next power of 2
        // and use it to determine "core_bits".

        topo_core_bits = (info.ecx >> 12) & 0xF;

        if (topo_core_bits == 0) {
            topo_core_count = info.ecx & 0xFF;
            topo_core_bits = bit_log2(topo_core_count);
        }

        topo_thread_bits = bit_log2(topo_thread_count >> topo_core_bits);
        topo_thread_count = 1 << topo_thread_bits;
    } else {
        topo_core_count = topo_thread_count;
        topo_core_bits = bit_log2(topo_core_count);
        topo_thread_bits = 0;
        topo_thread_count = 1;
    }

    topo_cpu_count = apic_id_count;
}

static void apic_detect_topology_intel(void)
{
    cpuid_t info;

    if (!cpuid(&info, CPUID_TOPOLOGY1, 0)) {
        // Enable full CPUID
        uint64_t misc_enables = cpu_msr_get(CPU_MSR_MISC_ENABLE);
        if (misc_enables & (1L<<22)) {
            // Enable more CPUID support and retry
            misc_enables &= ~(1L<<22);
            cpu_msr_set(CPU_MSR_MISC_ENABLE, misc_enables);
        }
    }

    topo_thread_bits = 0;
    topo_core_bits = 0;
    topo_thread_count = 1;
    topo_core_count = 1;

    if (cpuid(&info, CPUID_INFO_FEATURES, 0)) {
        if ((info.edx >> 28) & 1) {
            // CPU supports hyperthreading

            // Thread count
            topo_thread_count = (info.ebx >> 16) & 0xFF;
            while ((1U << topo_thread_bits) < topo_thread_count)
                 ++topo_thread_bits;
        }

        if (cpuid(&info, CPUID_TOPOLOGY1, 0)) {
            topo_core_count = ((info.eax >> 26) & 0x3F) + 1;
            while ((1U << topo_core_bits) < topo_core_count)
                 ++topo_core_bits;
        }
    }

    if (topo_thread_bits >= topo_core_bits)
        topo_thread_bits -= topo_core_bits;
    else
        topo_thread_bits = 0;

    topo_thread_count /= topo_core_count;

    // Workaround strange occurrence of it calculating 0 threads
    if (topo_thread_count <= 0)
        topo_thread_count = 1;

    topo_cpu_count = apic_id_count * topo_core_count * topo_thread_count;
}

__aligned(16) char const vendor_intel[16] = "GenuineIntel";
__aligned(16) char const vendor_amd[16] = "AuthenticAMD";

static void apic_detect_topology(void)
{
    cpuid_t info;

    // Assume 1 thread per core, 1 core per package, 1 package per cpu
    // until proven otherwise
    topo_core_bits = 0;
    topo_core_count = 1;
    topo_thread_bits = 0;
    topo_thread_count = 1;
    topo_cpu_count = apic_id_count;

    if (unlikely(!cpuid(&info, 0, 0)))
        return;

    union vendor_str_t {
        char txt[16];
        uint32_t regs[4];
    } vendor_str;

    vendor_str.regs[0] = info.ebx;
    vendor_str.regs[1] = info.edx;
    vendor_str.regs[2] = info.ecx;
    vendor_str.regs[3] = 0;

    APIC_TRACE("Detected CPU: %s\n", vendor_str.txt);

    if (!memcmp(vendor_intel, vendor_str.txt, 16)) {
        apic_detect_topology_intel();
    } else if (!memcmp(vendor_amd, vendor_str.txt, 16)) {
        apic_detect_topology_amd();
    }
}

void apic_start_smp(void)
{
    // Start the timer here because interrupts are enable by now
    apic_configure_timer(APIC_LVT_DCR_BY_1,
                         apic_timer_freq / 60,
                         APIC_LVT_TR_MODE_PERIODIC,
                         INTR_APIC_TIMER);

    APIC_TRACE("%d CPUs\n", apic_id_count);

    if (!acpi_rsdt_addr)
        apic_detect_topology();
    else {
        // Treat as N cpu packages
        topo_cpu_count = apic_id_count;
        topo_core_bits = 0;
        topo_thread_bits = 0;
        topo_core_count = 1;
        topo_thread_count = 1;
    }

    gdt_init_tss(topo_cpu_count);
    gdt_load_tr(0);

    // See if there are any other CPUs to start
    if (topo_thread_count * topo_core_count == 1 &&
            apic_id_count == 1)
        return;

    // Read address of MP entry trampoline from boot sector
    //uint32_t *mp_trampoline_ptr = (uint32_t*)0x7c40;
    //		bootinfo_parameter(bootparam_t::ap_entry_point);
    uint32_t mp_trampoline_addr = //*mp_trampoline_ptr;
            (uint32_t)
            bootinfo_parameter(bootparam_t::ap_entry_point);
    uint32_t mp_trampoline_page = mp_trampoline_addr >> 12;

    cmos_prepare_ap();

    // Send INIT to all other CPUs
//    for (size_t i = 1; i < apic_id_count; ++i) {
        apic_send_command(0, APIC_CMD_DELIVERY_INIT |
                          APIC_CMD_DEST_MODE_LOGICAL |
                          APIC_CMD_DEST_TYPE_OTHER);
//        APIC_TRACE("Sending INIT IPI to APIC ID %#x\n", apic_id_list[i]);
//        __sync_synchronize();
//        apic_send_command(apic_id_list[i],
//                          APIC_CMD_DELIVERY_INIT |
//                          APIC_CMD_DEST_MODE_PHYSICAL |
//                          APIC_CMD_DEST_TYPE_BYID);
//    }
    APIC_TRACE("Done sending INIT IPIs\n");

    // 10ms delay
    nsleep(10000000);

    APIC_TRACE("%d hyperthread bits\n", topo_thread_bits);
    APIC_TRACE("%d core bits\n", topo_core_bits);

    APIC_TRACE("%d hyperthread count\n", topo_thread_count);
    APIC_TRACE("%d core count\n", topo_core_count);

    uint32_t smp_expect = 0;
    for (unsigned pkg = 0; pkg < apic_id_count; ++pkg) {
        APIC_TRACE("Package base APIC ID = %u\n", apic_id_list[pkg]);

        uint8_t total_cpus = topo_core_count *
                topo_thread_count *
                apic_id_count;
        uint32_t stagger = 16666666 / total_cpus;

        for (unsigned thread = 0;
             thread < topo_thread_count; ++thread) {
            for (unsigned core = 0; core < topo_core_count; ++core) {
                uint8_t target = apic_id_list[pkg] +
                        (thread | (core << topo_thread_bits));

                // Don't try to start BSP
                if (target == apic_id_list[0])
                    continue;

                APIC_TRACE("Sending SIPI to APIC ID %u, "
                           "trampoline page=%#x\n",
                           target, mp_trampoline_page);

                // Send SIPI to CPU
                apic_send_command(target,
                                  APIC_CMD_SIPI_PAGE_n(mp_trampoline_page) |
                                  APIC_CMD_DELIVERY_SIPI |
                                  APIC_CMD_DEST_TYPE_BYID |
                                  APIC_CMD_DEST_MODE_PHYSICAL);

                nsleep(stagger);

                ++smp_expect;

                cpu_wait_value(&thread_smp_running, smp_expect);
            }
        }
    }

    // Do per-cpu ACPI configuration
    apic_config_cpu();

    ioapic_irq_setcpu(0, 1);
}

uint32_t apic_timer_count(void)
{
    return apic->read32(APIC_REG_LVT_CCR);
}

//
// ACPI timer

//template<int size>
//class acpi_gas_accessor_embed_t : public acpi_gas_accessor_t {
//public:
//    typedef typename type_from_size<size, true>::type value_type;
//
//    acpi_gas_accessor_embed_t(uint64_t addr, uint8_t size)
//        : acpi_gas_accessor_t(size) {}
//};
//
//template<int size>
//class acpi_gas_accessor_smbus_t : public acpi_gas_accessor_t {
//public:
//    typedef type_from_size<size, true>::type value_type;
//
//    acpi_gas_accessor_smbus_t(uint64_t addr, uint8_t size)
//        : acpi_gas_accessor_t(size) {}
//};
//
//template<int size>
//class acpi_gas_accessor_fixed_t : public acpi_gas_accessor_t {
//public:
//    typedef type_from_size<size, true>::type value_type;
//
//    acpi_gas_accessor_fixed_t(uint64_t addr, uint8_t size)
//        : acpi_gas_accessor_t(size) {}
//};

static uint64_t acpi_pm_timer_nsleep_handler(uint64_t nanosec);

// Returns -1 if PM timer is not available, otherwise a 24 or 32 bit raw value
static int64_t acpi_pm_timer_raw()
{
    static acpi_gas_accessor_t *accessor;

    if (unlikely(!accessor &&
                 (acpi_fadt.pm_timer_block ||
                  acpi_fadt.x_pm_timer_block.access_size))) {
        if (likely(acpi_fadt.pm_timer_block)) {
            ACPI_TRACE("PM Timer at I/O port %#x\n", acpi_fadt.pm_timer_block);
            accessor = acpi_gas_accessor_t::from_ioport(
                        acpi_fadt.pm_timer_block, 4, 0, 32);
        } else if (acpi_fadt.x_pm_timer_block.access_size) {
            acpi_gas_t const& gas = acpi_fadt.x_pm_timer_block;
            ACPI_TRACE("Using extended PM Timer Generic Address Structure: "
                       " addr_space: %#x, addr=%#.16" PRIx64 ", size=%#x,"
                       " width=%#x, bit=%#x\n",
                       gas.addr_space,
                       (uint64_t(gas.addr_hi) << 32) | gas.addr_lo,
                       gas.access_size, gas.bit_width, gas.bit_offset);
            accessor = acpi_gas_accessor_t::from_gas(gas);
        }

        if (likely(accessor))
            nsleep_set_handler(acpi_pm_timer_nsleep_handler, nullptr, true);
    }

    if (likely(accessor))
        return accessor->read();

    return -1;
}

static uint32_t acpi_pm_timer_diff(uint32_t before, uint32_t after)
{
    // If counter is 32 bits
    if (likely(acpi_fadt.flags & ACPI_FADT_FFF_TMR_VAL_EXT))
        return after - before;

    // Counter is 24 bits
    return ((after << 8) - (before << 8)) >> 8;
}

// Timer precision is approximately 279ns
static uint64_t acpi_pm_timer_ns(uint32_t diff)
{
    return (uint64_t(diff) * 1000000000) / ACPI_PM_TIMER_HZ;
}

_used
static uint64_t acpi_pm_timer_nsleep_handler(uint64_t ns)
{
    uint32_t st = acpi_pm_timer_raw();
    uint32_t en;
    uint32_t elap;
    uint32_t elap_ns;
    do {
        en = acpi_pm_timer_raw();
        elap = acpi_pm_timer_diff(st, en);
        elap_ns = acpi_pm_timer_ns(elap);
    } while (elap_ns < ns);

    return elap_ns;
}

template<typename T>
constexpr T gcd(T a, T b)
{
    T tmp;
    while (b) {
        tmp = a;
        a = b;
        b = tmp % b;
    }
    return a;
}

static uint64_t apic_rdtsc_time_ns_handler()
{
    uint64_t now = cpu_rdtsc();
    return now * clk_to_ns_numer/ clk_to_ns_denom;
}

static uint64_t apic_rdtsc_nsleep_handler(uint64_t nanosec)
{
    assert(clk_to_ns_denom != 0);
    assert(clk_to_ns_numer != 0);

    uint64_t begin = cpu_rdtsc();
    uint64_t then = nanosec * clk_to_ns_denom / clk_to_ns_numer + begin;
    uint64_t now;

    for (now = begin; now < then; now = cpu_rdtsc())
        pause();

    return (now - begin) * clk_to_ns_numer / clk_to_ns_denom;
}

static void apic_calibrate()
{
    cpuid_t info{};
    if (cpuid_is_hypervisor() && cpuid(&info, 0x40000000, 0) &&
            info.eax >= 0x40000010 && cpuid(&info, 0x40000010, 0)) {
        rdtsc_mhz = (info.eax + 500) / 1000;
        apic_timer_freq = info.ebx * UINT64_C(1000);
    } else if (acpi_pm_timer_raw() >= 0) {
        //
        // Have PM timer

        // Program timer (should be high enough to measure 858ms @ 5GHz)
        apic_configure_timer(APIC_LVT_DCR_BY_1, 0xFFFFFFF0U,
                             APIC_LVT_TR_MODE_n(APIC_LVT_TR_MODE_ONESHOT),
                             INTR_APIC_TIMER, true);

        uint32_t tmr_st = acpi_pm_timer_raw();
        uint32_t ccr_st = apic->read32(APIC_REG_LVT_CCR);
        uint32_t tmr_en;
        uint64_t tsc_st = cpu_rdtsc();
        uint32_t tmr_diff;

        // Wait for about 1ms
        do {
            pause();
            tmr_en = acpi_pm_timer_raw();
            tmr_diff = acpi_pm_timer_diff(tmr_st, tmr_en);
        } while (tmr_diff < 35790);

        uint32_t ccr_en = apic->read32(APIC_REG_LVT_CCR);
        uint64_t tsc_en = cpu_rdtsc();

        uint64_t tsc_elap = tsc_en - tsc_st;
        uint64_t ccr_elap = ccr_st - ccr_en;
        uint64_t tmr_nsec = acpi_pm_timer_ns(tmr_diff);

        uint64_t cpu_freq = (tsc_elap * 1000000000) / tmr_nsec;
        uint64_t ccr_freq = (ccr_elap * 1000000000) / tmr_nsec;

        apic_timer_freq = ccr_freq;

        // Round APIC frequency to nearest multiple of 333kHz
        apic_timer_freq += 166666;
        apic_timer_freq -= apic_timer_freq % 333333;

        // APIC frequency < 333kHz is impossible to believe
        assert(apic_timer_freq > 333333);

        // Round CPU frequency to nearest multiple of 1MHz
        cpu_freq += 500000;
        rdtsc_mhz = cpu_freq / 1000000;
    } else {
        // Program timer (should be high enough to measure 858ms @ 5GHz)
        apic_configure_timer(APIC_LVT_DCR_BY_1, 0xFFFFFFF0U,
                             APIC_LVT_TR_MODE_n(APIC_LVT_TR_MODE_ONESHOT),
                             INTR_APIC_TIMER, true);

        uint32_t ccr_st = apic->read32(APIC_REG_LVT_CCR);
        uint64_t tsc_st = cpu_rdtsc();

        // Wait for about 1ms
        uint64_t tmr_nsec = nsleep(10000000);

        uint32_t ccr_en = apic->read32(APIC_REG_LVT_CCR);
        uint64_t tsc_en = cpu_rdtsc();

        uint64_t tsc_elap = tsc_en - tsc_st;
        uint32_t ccr_elap = ccr_st - ccr_en;

        uint64_t cpu_freq = (tsc_elap * 1000000000) / tmr_nsec;
        uint64_t ccr_freq = (ccr_elap * 1000000000) / tmr_nsec;

        apic_timer_freq = ccr_freq;
        rdtsc_mhz = (cpu_freq + 500000) / 1000000;
    }

    // Example: let rdtsc_mhz = 2500. gcd(1000,2500) = 500
    // then,
    //  clk_to_ns_numer = 1000/500 = 2
    //  chk_to_ns_denom = 2500/500 = 5
    // clk_to_ns: let clks = 2500000000
    //  2500000000 * 2 / 5 = 1000000000ns

    uint64_t clk_to_ns_gcd = gcd(uint64_t(1000), rdtsc_mhz);

    APIC_TRACE("CPU MHz GCD: %" PRId64 "\n", clk_to_ns_gcd);

    clk_to_ns_numer = 1000 / clk_to_ns_gcd;
    clk_to_ns_denom = rdtsc_mhz / clk_to_ns_gcd;

    APIC_TRACE("clk_to_ns_numer: %" PRId64 "\n", clk_to_ns_numer);
    APIC_TRACE("clk_to_ns_denom: %" PRId64 "\n", clk_to_ns_denom);

    if (cpuid_has_inrdtsc()) {
        APIC_TRACE("Using RDTSC for precision timing\n");
        time_ns_set_handler(apic_rdtsc_time_ns_handler, nullptr, true);
        nsleep_set_handler(apic_rdtsc_nsleep_handler, nullptr, true);
    }

    APIC_TRACE("CPU clock: %" PRIu64 "MHz\n", rdtsc_mhz);
    APIC_TRACE("APIC clock: %" PRIu64 "Hz\n", apic_timer_freq);
}

//
// IOAPIC

static uint32_t ioapic_read(
        mp_ioapic_t *ioapic, uint32_t reg, mp_ioapic_t::scoped_lock const&)
{
    ioapic->ptr[IOAPIC_IOREGSEL] = reg;
    return ioapic->ptr[IOAPIC_IOREGWIN];
}

static void ioapic_write(mp_ioapic_t *ioapic, uint32_t reg, uint32_t value,
        const mp_ioapic_t::scoped_lock &)
{
    ioapic->ptr[IOAPIC_IOREGSEL] = reg;
    ioapic->ptr[IOAPIC_IOREGWIN] = value;
}

static mp_ioapic_t *ioapic_by_id(uint8_t id)
{
    for (unsigned i = 0; i < ioapic_count; ++i) {
        if (ioapic_list[i].id == id)
            return ioapic_list + i;
    }
    return nullptr;
}

static void ioapic_reset(mp_ioapic_t *ioapic)
{
    mp_ioapic_t::scoped_lock lock(ioapic->lock);

    // If this is the first IOAPIC, initialize some lookup tables
    if (ioapic_count == 1) {
        std::fill_n(intr_to_irq, countof(irq_to_intr), -1);
        std::fill_n(irq_to_intr, countof(irq_to_intr), -1);
        std::fill_n(intr_to_ioapic, countof(intr_to_ioapic), -1);
    }

    // Fill entries in interrupt-to-ioapic lookup table
    std::fill_n(intr_to_ioapic + ioapic->base_intr,
           ioapic->vector_count, ioapic - ioapic_list);

    // Assume GSIs are sequentially assigned to IOAPIC inputs
    for (size_t i = 0; i < ioapic->vector_count; ++i) {
        intr_to_irq[ioapic->base_intr + i] =
                ioapic->base_intr + i - INTR_APIC_IRQ_BASE;

        irq_to_intr[ioapic->base_intr +
                i - INTR_APIC_IRQ_BASE] = ioapic->base_intr + i;

        // Mask all IRQs
        uint32_t ent = ioapic_read(ioapic, IOAPIC_RED_LO_n(i), lock);

        // Mask interrupts
        IOAPIC_REDLO_MASKIRQ_SET(ent, 1);
        ioapic_write(ioapic, IOAPIC_RED_LO_n(i), ent, lock);

        // Set vector
        IOAPIC_REDLO_VECTOR_SET(ent, ioapic->base_intr + i);
        ioapic_write(ioapic, IOAPIC_RED_LO_n(i), ent, lock);

        // Route to CPU 0 by default
        ioapic_write(ioapic, IOAPIC_RED_HI_n(i), IOAPIC_REDHI_DEST_n(0), lock);
    }
}

static void ioapic_set_type(mp_ioapic_t *ioapic,
                            uint8_t intin, uint8_t intr_type)
{
    mp_ioapic_t::scoped_lock lock(ioapic->lock);

    uint32_t reg = ioapic_read(ioapic, IOAPIC_RED_LO_n(intin), lock);

    switch (intr_type) {
    case MP_INTR_TYPE_APIC:
        IOAPIC_REDLO_DELIVERY_SET(reg, IOAPIC_REDLO_DELIVERY_APIC);
        break;

    case MP_INTR_TYPE_NMI:
        IOAPIC_REDLO_DELIVERY_SET(reg, IOAPIC_REDLO_DELIVERY_NMI);
        break;

    case MP_INTR_TYPE_SMI:
        IOAPIC_REDLO_DELIVERY_SET(reg, IOAPIC_REDLO_DELIVERY_SMI);
        break;

    case MP_INTR_TYPE_EXTINT:
        IOAPIC_REDLO_DELIVERY_SET(reg, IOAPIC_REDLO_DELIVERY_EXTINT);
        break;

    default:
        IOAPIC_TRACE("Unrecognized delivery type %d!\n", intr_type);
        return;
    }

    ioapic_write(ioapic, IOAPIC_RED_LO_n(intin), reg, lock);
}

static void ioapic_set_flags(mp_ioapic_t *ioapic,
                             uint8_t intin, uint16_t intr_flags, bool isa)
{
    mp_ioapic_t::scoped_lock lock(ioapic->lock);

    uint32_t reg = ioapic_read(ioapic, IOAPIC_RED_LO_n(intin), lock);

    uint16_t polarity = ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_GET(intr_flags);
    uint16_t trigger = ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_GET(intr_flags);

    if (polarity == ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_DEFAULT)
        polarity = isa ? ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVEHI
                       : ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVELO;

    if (trigger == ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_DEFAULT)
        trigger = isa ? ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_EDGE
                      : ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_LEVEL;

    switch (polarity) {
    default:
        APIC_TRACE("MP: Unrecognized IRQ polarity type!"
                   " Guessing active low\n");
        // fall through

    case ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVELO:
        IOAPIC_REDLO_POLARITY_SET(reg, IOAPIC_REDLO_POLARITY_ACTIVELO);
        break;

    case ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVEHI:
        IOAPIC_REDLO_POLARITY_SET(reg, IOAPIC_REDLO_POLARITY_ACTIVEHI);
        break;
    }

    switch (trigger) {
    default:
        APIC_TRACE("MP: Unrecognized IRQ trigger type!"
                   " Guessing edge\n");
        // fall through...

    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_EDGE:
        IOAPIC_REDLO_TRIGGER_SET(reg, IOAPIC_REDLO_TRIGGER_EDGE);
        break;

    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_LEVEL:
        IOAPIC_REDLO_TRIGGER_SET(reg, IOAPIC_REDLO_TRIGGER_LEVEL);
        break;

    }

    ioapic_write(ioapic, IOAPIC_RED_LO_n(intin), reg, lock);
}

// Call on each CPU to configure local APIC using gathered MPS/ACPI records
void apic_config_cpu()
{
    int cpu_number = thread_cpu_number();
    uint32_t apic_id = thread_get_cpu_apic_id(cpu_number);

    // Apply NMI input delivery types
    for (acpi_madt_lnmi_t const& mapping : lapic_lint_nmi) {
        // 0xFF entries apply to all CPUs
        // Skip mappings for other CPUs
        if (mapping.apic_id != 0xFF && mapping.apic_id != apic_id)
            continue;

        unsigned reg_ofs;

        switch (mapping.lapic_lint) {
        case 0:
            reg_ofs = APIC_REG_LVT_LNT0;
            break;
        case 1:
            reg_ofs = APIC_REG_LVT_LNT1;
            break;

        default:
            ACPI_TRACE("Out of range LNMI lint: %u\n", mapping.lapic_lint);
            continue;

        }

        uint32_t reg = apic->read32(reg_ofs);
        APIC_LVT_DELIVERY_SET(reg, APIC_LVT_DELIVERY_NMI);
        apic->write32(reg_ofs, reg);
    }
}

//
//

isr_context_t *apic_dispatcher(int intr, isr_context_t *ctx)
{
    assert(intr >= INTR_APIC_IRQ_BASE);
    assert(intr < INTR_APIC_IRQ_END);

    isr_context_t *orig_ctx = ctx;

    int irq = intr_to_irq[intr];

    assert(irq >= 0);
    assert(irq < INTR_APIC_IRQ_COUNT);

    ctx = irq_invoke(intr, irq, ctx);
    apic_eoi(intr);

    if (ctx == orig_ctx)
        return thread_schedule_if_idle(ctx);

    return ctx;
}

static void ioapic_setmask(int irq, bool unmask)
{
    int irq_intr = irq_to_intr[irq];
    assert(irq_intr >= 0);
    int ioapic_index = intr_to_ioapic[irq_intr];
    assert(ioapic_index >= 0);
    assert(ioapic_index < int(ioapic_count));
    mp_ioapic_t *ioapic = ioapic_list + ioapic_index;
    int intin = irq_intr - ioapic->base_intr;
    assert(intin >= 0 && intin < ioapic->vector_count);

    mp_ioapic_t::scoped_lock lock(ioapic->lock);

    uint32_t ent = ioapic_read(ioapic, IOAPIC_RED_LO_n(intin), lock);

    if (unmask)
        ent &= ~IOAPIC_REDLO_MASKIRQ;
    else
        ent |= IOAPIC_REDLO_MASKIRQ;

    ioapic_write(ioapic, IOAPIC_RED_LO_n(intin), ent, lock);
}

static void ioapic_hook(int irq, intr_handler_t handler,
                        char const *name)
{
    int intr = irq_to_intr[irq];
    assert(intr >= 0);
    if (intr >= 0) {
        intr_hook(intr, handler, name);
    } else {
        APIC_TRACE("Hooking %s IRQ %d failed, irq_to_intr[%d]==%d\n",
                   name, irq, irq, intr);
    }
}

static void ioapic_unhook(int irq, intr_handler_t handler)
{
    int intr = irq_to_intr[irq];
    assert(intr >= 0);
    intr_unhook(intr, handler);
}

static void ioapic_map_all(void)
{
    // MSI IRQs start after last IOAPIC
    mp_ioapic_t *ioapic = &ioapic_list[ioapic_count-1];
    ioapic_msi_base_intr = ioapic->base_intr + ioapic->vector_count;
    ioapic_msi_base_irq = ioapic->irq_base + ioapic->vector_count;
}

// Returns 0 on failure, 1 on success
bool ioapic_irq_setcpu(int irq, int cpu)
{
    if (unsigned(cpu) >= apic_id_count)
        return false;

    int irq_intr = irq_to_intr[irq];
    int ioapic_index = intr_to_ioapic[irq_intr];
    mp_ioapic_t *ioapic = ioapic_list + ioapic_index;
    mp_ioapic_t::scoped_lock lock(ioapic->lock);
    unsigned intin = irq_intr - ioapic->base_intr;
    uint32_t lo;
    uint32_t hi;

    if (cpu >= 0) {
        hi = ioapic_read(ioapic, IOAPIC_RED_HI_n(intin), lock);
        IOAPIC_REDHI_DEST_SET(hi, apic_id_list[cpu]);
        ioapic_write(ioapic, IOAPIC_RED_HI_n(intin), hi, lock);
    } else {
        // Read current values
        lo = ioapic_read(ioapic, IOAPIC_RED_LO_n(intin), lock);
        hi = ioapic_read(ioapic, IOAPIC_RED_HI_n(intin), lock);

        // Modify fields
        IOAPIC_REDLO_DELIVERY_SET(lo, IOAPIC_REDLO_DELIVERY_LOWPRI);
        IOAPIC_REDLO_DESTMODE_SET(lo, IOAPIC_REDLO_DESTMODE_LOGICAL);
        IOAPIC_REDHI_DEST_SET(hi, 0xFF);

        // Write it back masked
        ioapic_write(ioapic, IOAPIC_RED_LO_n(intin),
                     lo | IOAPIC_REDLO_MASKIRQ, lock);
        ioapic_write(ioapic, IOAPIC_RED_HI_n(intin), hi, lock);

        // If not masked, unmask after completing changes
        if (!(lo & IOAPIC_REDLO_MASKIRQ))
            ioapic_write(ioapic, IOAPIC_RED_LO_n(intin), lo, lock);
    }
    return true;
}

int apic_enable(void)
{
    if (ioapic_count == 0)
        return 0;

    ioapic_map_all();

    irq_setmask_set_handler(ioapic_setmask);
    irq_hook_set_handler(ioapic_hook);
    irq_unhook_set_handler(ioapic_unhook);
    irq_setcpu_set_handler(ioapic_irq_setcpu);

    return 1;
}

//
// MSI IRQ

void apic_msi_target(msi_irq_mem_t *result, int cpu, int vector)
{
    int cpu_count = thread_cpu_count();
    assert(cpu >= -1);
    assert(cpu < cpu_count);
    assert(vector >= INTR_APIC_IRQ_BASE);
    assert(vector < INTR_APIC_IRQ_END);

    // Intel's ridiculous documentation for DM and RH fields:
    ///  This bit indicates whether the Destination ID field should be
    ///  interpreted as logical or physical APIC ID for delivery of the
    ///  lowest priority interrupt. If RH is 1 and DM is 0, the Destination ID
    ///  field is in physical destination mode and only the processor in the
    ///  system that has the matching APIC ID is considered for delivery of
    ///  that interrupt (this means no re-direction). If RH is 1 and DM is 1,
    ///  the Destination ID Field is interpreted as in logical destination
    ///  mode and the redirection is limited to only those processors that are
    ///  part of the logical group of processors based on the processor’s
    ///  logical APIC ID and the Destination ID field in the message. The
    ///  logical group of processors consists of those identified by matching
    ///  the 8-bit Destination ID with the logical destination identified by
    ///  the Destination Format Register and the Logical Destination Register
    ///  in each local APIC. The details are similar to those described in
    ///  Section 10.6.2, “Determining IPI Destination.”
    ///  If RH is 0, then the DM bit is ignored and the message is sent ahead
    ///  independent of whether the physical or logical destination mode
    ///  is used.
    ///
    /// It is ridiculous because:
    ///
    ///  1) It does not specify what "sent ahead" means. Sent ahead to where?
    ///  2) It says that the handling of LDR and DFR are "similar" to
    ///     another section. Similar?
    ///
    /// Translation:
    ///
    /// +----+----+---------------------------------------------------------+
    /// | RH | DM |                                                         |
    /// |bit3|bit2|                                                         |
    /// +----+----+---------------------------------------------------------+
    /// |  0 |  x | Apparently "sent ahead", no idea what that means        |
    /// |  1 |  0 | Physical APIC ID                                        |
    /// |  1 |  1 | Target CPU based on DFR and LDR in each LAPIC           |
    /// +----+----+---------------------------------------------------------+

    if (cpu >= 0) {
        // Specific CPU
        result->addr = (0xFEEU << 20) |
                ((apic_id_list[cpu % cpu_count] & 0xFF) << 12);
    } else {
        // Lowest priority
        result->addr = (0xFEEU << 20) | (1 << 2) | (1 << 3) |
                (0xFF << 12);
    }
    result->data = vector;
}

// Returns the starting IRQ number of allocated range
// If cpu is -1, then enable lowest priority mode
// target_cpus must be null or an array of count 'count'
// vector_offsets must be null or an array of count 'count'
// vector_offsets must be null if not using MSIX
// Returns 0 for failure
int apic_msi_irq_alloc(msi_irq_mem_t *results, int count,
                       int cpu, bool distribute,
                       intr_handler_t handler, char const *name,
                       int const *target_cpus, int const *vector_offsets,
                       bool aligned)
{
    // Don't try to use MSI if there are no IOAPIC devices
    if (ioapic_count == 0) {
        APIC_TRACE("Can't allocate MSI interrupts, no APIC devices\n");
        return 0;
    }

    // If out of range starting CPU number, force to zero
    if (cpu < -1 || unsigned(cpu) >= apic_id_count)
        cpu = 0;

    int vector_cnt;
    uint8_t vector_base;

    // If using MSIX and a vector offset table was passed...
    if (vector_offsets) {
        // ...compute how many vectors to allocate from vector_offsets
        vector_cnt = pci_vector_count_from_offsets(vector_offsets, count);

        // Allocate only as many vectors as we need per vector_offsets
        vector_base = ioapic_alloc_vectors(vector_cnt);
    } else {
        vector_cnt = count;
        if (aligned) {
            // Allocate power of two count of suitably aligned vectors
            vector_base = ioapic_aligned_vectors(bit_log2(count));
        } else {
            vector_base = ioapic_alloc_vectors(count);
        }
    }

    // See if we ran out of vectors
    if (vector_base == 0)
        return 0;

    for (int i = 0; i < count; ++i) {
        int vec_ofs = vector_offsets ? vector_offsets[i] : i;
        int target_cpu = target_cpus ? target_cpus[i] : cpu;

        apic_msi_target(results + i, target_cpu, vector_base + vec_ofs);

        uint8_t intr = vector_base + vec_ofs;
        uint8_t irq = vector_base + vec_ofs - INTR_APIC_IRQ_BASE;

        APIC_TRACE("%s msi(x) IRQ %u = vector %u (%#x), cpu=%d"
                   ", addr=%#zx, data=%#zx\n",
                   name, irq, intr, intr, target_cpu,
                   results[i].addr, results[i].data);

        irq_to_intr[irq] = intr;
        intr_to_irq[intr] = irq;

        ioapic_hook(irq, handler, name);

        if (distribute && cpu >= 0) {
            if (++cpu >= int(apic_id_count))
                cpu = 0;
        }

        // It will always be edge triggered
        // (!!activehi << 14) |
        // (!!level << 15);
    }

    return vector_base - INTR_APIC_IRQ_BASE;
}

uint32_t acpi_cpu_count()
{
    return apic_id_count;
}
