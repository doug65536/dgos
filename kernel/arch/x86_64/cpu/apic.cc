#include "apic.h"
#include "types.h"
#include "gdt.h"
#include "bios_data.h"
#include "control_regs.h"
#include "interrupts.h"
#include "irq.h"
#include "idt.h"
#include "thread_impl.h"
#include "mm.h"
#include "cpuid.h"
#include "string.h"
#include "atomic.h"
#include "printk.h"
#include "likely.h"
#include "time.h"
#include "cpuid.h"
#include "spinlock.h"
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
#include "apicbits.h"
#include "mutex.h"

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

// entry_type 3 and 4 flags

#define MP_INTR_FLAGS_POLARITY_DEFAULT    0
#define MP_INTR_FLAGS_POLARITY_ACTIVEHI   1
#define MP_INTR_FLAGS_POLARITY_ACTIVELO   3

#define MP_INTR_FLAGS_TRIGGER_DEFAULT     0
#define MP_INTR_FLAGS_TRIGGER_EDGE        1
#define MP_INTR_FLAGS_TRIGGER_LEVEL       3

#define MP_INTR_TYPE_APIC   0
#define MP_INTR_TYPE_NMI    1
#define MP_INTR_TYPE_SMI    2
#define MP_INTR_TYPE_EXTINT 3

//
// IOAPIC registers

#define IOAPIC_IOREGSEL         0
#define IOAPIC_IOREGWIN         4

#define IOAPIC_REG_ID           0
#define IOAPIC_REG_VER          1
#define IOAPIC_REG_ARB          2

#define IOAPIC_RED_LO_n(n)      (0x10 + (n) * 2)
#define IOAPIC_RED_HI_n(n)      (0x10 + (n) * 2 + 1)

#define IOAPIC_REDLO_DELIVERY_APIC      0
#define IOAPIC_REDLO_DELIVERY_LOWPRI    1
#define IOAPIC_REDLO_DELIVERY_SMI       2
#define IOAPIC_REDLO_DELIVERY_NMI       4
#define IOAPIC_REDLO_DELIVERY_INIT      5
#define IOAPIC_REDLO_DELIVERY_EXTINT    7

#define IOAPIC_REDLO_TRIGGER_EDGE   0
#define IOAPIC_REDLO_TRIGGER_LEVEL  1

#define IOAPIC_REDLO_POLARITY_ACTIVELO  1
#define IOAPIC_REDLO_POLARITY_ACTIVEHI  0

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
    spinlock lock;
};

static char *mp_tables;

static vector<uint8_t> mp_pci_bus_ids;
static uint16_t mp_isa_bus_id;

static uint64_t apic_timer_freq;

static unsigned ioapic_count;
static mp_ioapic_t ioapic_list[16];

static spinlock ioapic_msi_alloc_lock;
static uint8_t ioapic_msi_next_irq = INTR_APIC_IRQ_BASE;
static uint64_t ioapic_msi_alloc_map[] = {
    0x0000000000000000L,
    0x0000000000000000L,
    0x0000000000000000L,
    0x0000000000000000L
};

static mp_ioapic_t *ioapic_by_id(uint8_t id);
static void ioapic_reset(mp_ioapic_t *ioapic);
static void ioapic_set_flags(mp_ioapic_t *ioapic,
                             uint8_t intin, uint16_t intr_flags);
static uint32_t ioapic_read(
        mp_ioapic_t *ioapic, uint32_t reg, unique_lock<spinlock> const&);
static void ioapic_write(
        mp_ioapic_t *ioapic, uint32_t reg, uint32_t value,
        unique_lock<spinlock> const&);

static uint8_t ioapic_next_irq_base = 16;

// First vector after last IOAPIC
static uint8_t ioapic_msi_base_intr;
static uint8_t ioapic_msi_base_irq;

static vector<uint8_t> isa_irq_lookup;

static unsigned apic_id_count;
static uint8_t apic_id_list[64];

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

#define MP_TABLE_TYPE_CPU           0
#define MP_TABLE_TYPE_BUS           1
#define MP_TABLE_TYPE_IOAPIC        2
#define MP_TABLE_TYPE_IOINTR        3
#define MP_TABLE_TYPE_LINTR         4
#define MP_TABLE_TYPE_ADDRMAP       128
#define MP_TABLE_TYPE_BUSHIER       129
#define MP_TABLE_TYPE_BUSCOMPAT     130

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

#define APIC_SIR_APIC_ENABLE_BIT    8
#define APIC_SIR_APIC_ENABLE        (1<<APIC_SIR_APIC_ENABLE_BIT)

#define APIC_LVT_TR_MODE_ONESHOT    0
#define APIC_LVT_TR_MODE_PERIODIC   1
#define APIC_LVT_TR_MODE_DEADLINE   2

#define APIC_LVT_DELIVERY_FIXED     0
#define APIC_LVT_DELIVERY_SMI       2
#define APIC_LVT_DELIVERY_NMI       4
#define APIC_LVT_DELIVERY_EXINT     7
#define APIC_LVT_DELIVERY_INIT      5

#define APIC_BASE_MSR               0x1B

#define APIC_BASE_ADDR_BIT          12
#define APIC_BASE_ADDR_BITS         40
#define APIC_BASE_GENABLE_BIT       11
#define APIC_BASE_X2ENABLE_BIT      10
#define APIC_BASE_BSP_BIT           8

#define APIC_BASE_GENABLE           (1UL<<APIC_BASE_GENABLE_BIT)
#define APIC_BASE_X2ENABLE          (1UL<<APIC_BASE_X2ENABLE_BIT)
#define APIC_BASE_BSP               (1UL<<APIC_BASE_BSP_BIT)
#define APIC_BASE_ADDR_MASK         ((1UL<<APIC_BASE_ADDR_BITS)-1)
#define APIC_BASE_ADDR              (APIC_BASE_ADDR_MASK<<APIC_BASE_ADDR_BIT)

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
} __packed;

C_ASSERT(sizeof(acpi_sdt_hdr_t) == 36);

// Generic Address Structure
struct acpi_gas_t {
    uint8_t addr_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint32_t addr_lo;
    uint32_t addr_hi;
};

C_ASSERT(sizeof(acpi_gas_t) == 12);

#define ACPI_GAS_ADDR_SYSMEM    0
#define ACPI_GAS_ADDR_SYSIO     1
#define ACPI_GAS_ADDR_PCICFG    2
#define ACPI_GAS_ADDR_EMBED     3
#define ACPI_GAS_ADDR_SMBUS     4
#define ACPI_GAS_ADDR_FIXED     0x7F

#define ACPI_GAS_ASZ_UNDEF  0
#define ACPI_GAS_ASZ_8      0
#define ACPI_GAS_ASZ_16     0
#define ACPI_GAS_ASZ_32     0
#define ACPI_GAS_ASZ_64     0

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
} __packed;

struct acpi_ssdt_t {
    // sig == ?
    acpi_sdt_hdr_t hdr;
} __packed;

struct acpi_madt_rec_hdr_t {
    uint8_t entry_type;
    uint8_t record_len;
};

#define ACPI_MADT_REC_TYPE_LAPIC    0
#define ACPI_MADT_REC_TYPE_IOAPIC   1
#define ACPI_MADT_REC_TYPE_IRQ      2

struct acpi_madt_lapic_t {
    acpi_madt_rec_hdr_t hdr;
    uint8_t cpu_id;
    uint8_t apic_id;
    uint32_t flags;
} __packed;

struct acpi_madt_ioapic_t {
    acpi_madt_rec_hdr_t hdr;
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t addr;
    uint32_t irq_base;
} __packed;

struct acpi_madt_irqsrc_t {
    acpi_madt_rec_hdr_t hdr;
    uint8_t bus;
    uint8_t irq_src;
    uint32_t gsi;
    uint16_t flags;
} __packed;

//
// The IRQ routing flags are identical to MPS flags

#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_BIT \
    MP_INTR_FLAGS_POLARITY_BIT
#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_BITS \
    MP_INTR_FLAGS_POLARITY_BITS
#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_BIT \
    MP_INTR_FLAGS_TRIGGER_BIT
#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_BITS \
    MP_INTR_FLAGS_TRIGGER_BITS

#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_MASK \
    MP_INTR_FLAGS_TRIGGER_MASK

#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_MASK \
    MP_INTR_FLAGS_POLARITY_MASK

#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER \
    MP_INTR_FLAGS_TRIGGER

#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY \
    MP_INTR_FLAGS_POLARITY

#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_DEFAULT \
    MP_INTR_FLAGS_POLARITY_DEFAULT
#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVEHI \
    MP_INTR_FLAGS_POLARITY_ACTIVEHI
#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVELO \
    MP_INTR_FLAGS_POLARITY_ACTIVELO

#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_DEFAULT \
    MP_INTR_FLAGS_TRIGGER_DEFAULT
#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_EDGE \
    MP_INTR_FLAGS_TRIGGER_EDGE
#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_LEVEL \
    MP_INTR_FLAGS_TRIGGER_LEVEL

#define ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_n(n) \
    MP_INTR_FLAGS_POLARITY_n(n)
#define ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(n) \
    MP_INTR_FLAGS_TRIGGER_n(n)

union acpi_madt_ent_t {
    acpi_madt_rec_hdr_t hdr;
    acpi_madt_lapic_t lapic;
    acpi_madt_ioapic_t ioapic;
    acpi_madt_irqsrc_t irq_src;
} __packed;

struct acpi_madt_t {
    // sig == "APIC"
    acpi_sdt_hdr_t hdr;

    uint32_t lapic_address;

    // 1 = Dual 8259 PICs installed
    uint32_t flags;
} __packed;

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

static vector<acpi_gas_t> acpi_hpet_list;
static int acpi_madt_flags;

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
    unique_lock<spinlock> lock(ioapic_msi_alloc_lock);

    uint8_t base = ioapic_msi_next_irq;
    ioapic_msi_next_irq += count;

    for (size_t intr = base - INTR_APIC_IRQ_BASE, end = intr + count;
         intr < end; ++intr) {
        ioapic_msi_alloc_map[intr >> 6] |= (1UL << (intr & 0x3F));
    }

    return base;
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

    unique_lock<spinlock> lock(ioapic_msi_alloc_lock);

    for (size_t bit = 0; bit < 128; bit += count)
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

static __always_inline uint8_t checksum_bytes(char const *bytes, size_t len)
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

static void acpi_process_madt(acpi_madt_t *madt_hdr)
{
    acpi_madt_ent_t *ent = (acpi_madt_ent_t*)(madt_hdr + 1);
    acpi_madt_ent_t *end = (acpi_madt_ent_t*)
            ((char*)madt_hdr + madt_hdr->hdr.len);

    apic_base = madt_hdr->lapic_address;
    acpi_madt_flags = madt_hdr->flags & 1;

    for ( ; ent < end;
          ent = (acpi_madt_ent_t*)((char*)ent + ent->ioapic.hdr.record_len)) {
        ACPI_TRACE("FADT record, type=%x ptr=%p, len=%u\n",
                   ent->hdr.entry_type, (void*)ent,
                   ent->ioapic.hdr.record_len);
        switch (ent->hdr.entry_type) {
        case ACPI_MADT_REC_TYPE_LAPIC:
            if (apic_id_count < countof(apic_id_list)) {
                ACPI_TRACE("Found LAPIC, ID=%d\n", ent->lapic.apic_id);

                // If processor is enabled
                if (ent->lapic.flags == 1) {
                    apic_id_list[apic_id_count++] = ent->lapic.apic_id;
                } else {
                    ACPI_TRACE("Disabled processor detected\n");
                }
            } else {
                ACPI_ERROR("Too many CPU packages! Dropped one\n");
            }
            break;

        case ACPI_MADT_REC_TYPE_IOAPIC:
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
                entries >>= IOAPIC_VER_ENTRIES_BIT;
                entries &= IOAPIC_VER_ENTRIES_MASK;

                ioapic->vector_count = entries;
                ioapic->base_intr = ioapic_alloc_vectors(entries);

                ioapic_reset(ioapic);

                ACPI_TRACE("IOAPIC registered, id=%u, addr=0x%x, irqbase=%d,"
                           " entries=%u\n",
                           ioapic->id, ioapic->addr, ioapic->irq_base,
                           ioapic->vector_count);
            } else {
                ACPI_TRACE("Too many IOAPICs!\n");
            }
            break;

        case ACPI_MADT_REC_TYPE_IRQ:
        {
            ACPI_TRACE("Got Interrupt redirection table\n");

            uint8_t gsi = ent->irq_src.gsi;
            uint8_t intr = INTR_APIC_IRQ_BASE + gsi;
            uint8_t irq = ent->irq_src.irq_src;

            intr_to_irq[intr] = irq;
            irq_to_intr[irq] = intr;

            ACPI_TRACE("Applied redirection, irq=%u, gsi=%u, vector=%u\n",
                       irq, gsi, intr);

            break;
        }
        default:
            ACPI_TRACE("Unrecognized FADT record\n");
            break;

        }
    }
}

static void acpi_process_hpet(acpi_hpet_t *acpi_hdr)
{
    acpi_hpet_list.push_back(acpi_hdr->addr);
}

static __always_inline uint8_t acpi_chk_hdr(acpi_sdt_hdr_t *hdr)
{
    return checksum_bytes((char const *)hdr, hdr->len);
}

static bool acpi_find_rsdp(void *start, size_t len)
{
    for (size_t offset = 0; offset < len; offset += 16) {
        acpi_rsdp2_t *rsdp2 = (acpi_rsdp2_t*)
                ((char*)start + offset);

        // Check for ACPI 2.0+ RSDP
        if (!memcmp(rsdp2->rsdp1.sig, "RSD PTR ", 8)) {
            // Check checksum
            if (rsdp2->rsdp1.rev != 0 &&
                    checksum_bytes((char*)rsdp2,
                               sizeof(*rsdp2)) == 0 &&
                    checksum_bytes((char*)&rsdp2->rsdp1,
                                   sizeof(rsdp2->rsdp1)) == 0) {
                if (rsdp2->xsdt_addr_lo | rsdp2->xsdt_addr_hi) {
                    ACPI_TRACE("Found 64-bit XSDT\n");
                    acpi_rsdt_addr = rsdp2->xsdt_addr_lo |
                            ((uint64_t)rsdp2->xsdt_addr_hi << 32);
                    acpi_rsdt_ptrsz = sizeof(uint64_t);
                } else {
                    ACPI_TRACE("Found 32-bit RSDT\n");
                    acpi_rsdt_addr = rsdp2->rsdp1.rsdt_addr;
                    acpi_rsdt_ptrsz = sizeof(uint32_t);
                }

                acpi_rsdt_len = rsdp2->length;

                ACPI_TRACE("Found ACPI 2.0+ RSDP at 0x%zx\n", acpi_rsdt_addr);

                return true;
            }
        }

        // Check for ACPI 1.0 RSDP
        acpi_rsdp_t *rsdp = (acpi_rsdp_t*)rsdp2;
        if (rsdp->rev == 0 &&
                !memcmp(rsdp->sig, "RSD PTR ", 8)) {
            // Check checksum
            if (checksum_bytes((char*)rsdp, sizeof(*rsdp)) == 0) {
                acpi_rsdt_addr = rsdp->rsdt_addr;
                acpi_rsdt_ptrsz = sizeof(uint32_t);

                // Leave acpi_rsdt_len 0 in this case, it is
                // handled later

                ACPI_TRACE("Found ACPI 1.0 RSDP at 0x%zx\n", acpi_rsdt_addr);

                return true;
            }
        }
    }

    return false;
}

static bool mp_find_fps(void *start, size_t len)
{
    for (size_t offset = 0; offset < len; offset += 16) {
        mp_table_hdr_t *sig_srch = (mp_table_hdr_t*)
                ((char*)start + offset);

        // Check for MP tables signature
        if (!memcmp(sig_srch->sig, "_MP_", 4)) {
            // Check checksum
            if (checksum_bytes((char*)sig_srch,
                               sizeof(*sig_srch)) == 0) {
                mp_tables = (char*)(uintptr_t)sig_srch->phys_addr;

                ACPI_TRACE("Found MPS tables at %zx\n", size_t(mp_tables));

                return true;
            }
        }
    }

    return false;
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

    ACPI_TRACE("RSDT version %x\n", rsdt_hdr->rev);

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
                mmap((void*)(uintptr_t)hdr_addr,
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
        } else {
            if (acpi_chk_hdr(hdr) == 0) {
                ACPI_TRACE("ACPI %4.4s ignored\n", hdr->sig);
            } else {
                ACPI_ERROR("ACPI %4.4s checksum mismatch!"
                       " (ignored anyway)\n", hdr->sig);
            }
        }

        munmap(hdr, max(size_t(64 << 10), size_t(hdr->len)));
    }
}

static void mp_parse_fps()
{
    mp_cfg_tbl_hdr_t *cth = (mp_cfg_tbl_hdr_t *)
            mmap(mp_tables, 0x10000,
                 PROT_READ, MAP_PHYSICAL, -1, 0);

    cth = acpi_remap_len(cth, uintptr_t(mp_tables),
                         0x10000, cth->base_tbl_len + cth->ext_tbl_len);

    uint8_t *entry = (uint8_t*)(cth + 1);

    // Reset to impossible values
    mp_isa_bus_id = -1;

    // First slot reserved for BSP
    apic_id_count = 1;

    fill_n(intr_to_irq, countof(intr_to_irq), -1);
    fill_n(irq_to_intr, countof(irq_to_intr), -1);
    fill_n(intr_to_ioapic, countof(intr_to_ioapic), -1);

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
                      " base apic id=%u ver=0x%x\n",
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

                MPS_TRACE("IOAPIC id=%d, addr=0x%x,"
                          " flags=0x%x, ver=0x%x\n",
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
                            MAP_PHYSICAL |
                            MAP_NOCACHE |
                            MAP_WRITETHRU, -1, 0);

                ioapic->ptr = ioapic_ptr;

                // Read redirection table size

                ioapic_ptr[IOAPIC_IOREGSEL] = IOAPIC_REG_VER;
                uint32_t ioapic_ver = ioapic_ptr[IOAPIC_IOREGWIN];

                uint8_t ioapic_intr_count =
                        (ioapic_ver >> IOAPIC_VER_ENTRIES_BIT) &
                        IOAPIC_VER_ENTRIES_MASK;

                // Allocate virtual IRQ numbers
                ioapic->irq_base = ioapic_next_irq_base;
                ioapic_next_irq_base += ioapic_intr_count;

                // Allocate vectors, assign range to IOAPIC
                ioapic->vector_count = ioapic_intr_count;
                ioapic->base_intr = ioapic_alloc_vectors(
                            ioapic_intr_count);

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

            ioapic_set_flags(ioapic_by_id(ioapic_id), intin, intr_flags);

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
                          " LAPIC ID 0x%02x INTIN %d\n",
                          device, (int)(pci_irq & 3) + 'A',
                          lapic_id, intin);
            } else if (entry_lintr->source_bus == mp_isa_bus_id) {
                uint8_t isa_irq = entry_lintr->source_bus_irq;
                uint8_t lapic_id = entry_lintr->dest_lapic_id;
                uint8_t intin = entry_lintr->dest_lapic_lintin;

                MPS_TRACE("ISA IRQ %d -> LAPIC ID 0x%02x INTIN %u\n",
                          isa_irq, lapic_id, intin);
            } else {
                // Unknown bus!
                MPS_ERROR("IRQ %d on unknown bus ->"
                          " IOAPIC ID 0x%02x INTIN %u\n",
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

            MPS_TRACE("Address map, bus=%d, addr=%lx, len=%lx\n",
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

    munmap(cth, max(0x10000, cth->base_tbl_len + cth->ext_tbl_len));
}

static int parse_mp_tables(void)
{
    uint16_t ebda_seg = *BIOS_DATA_AREA(uint16_t, 0x40E);
    uintptr_t ebda = uintptr_t(ebda_seg << 4);

    // ACPI RSDP can be found:
    //  - in the first 1KB of the EBDA
    //  - in the 128KB range starting at 0xE0000

    // MP table floating pointer structure can be found:
    //  - in the first 1KB of the EBDA
    //  - in the 1KB range starting at 0x9FC00
    //  - in the 64KB range starting at 0xF0000

    void *p_9fc00 = mmap((void*)0x9FC00, 0x400, PROT_READ,
                         MAP_PHYSICAL, -1, 0);
    void *p_e0000 = mmap((void*)0xE0000, 0x20000, PROT_READ,
                       MAP_PHYSICAL, -1, 0);
    void *p_f0000 = (char*)p_e0000 + 0x10000;

    void *p_ebda;
    if (ebda == 0x9FC00) {
        p_ebda = p_9fc00;
    } else {
        p_ebda = mmap((void*)ebda, 0x400, PROT_READ,
                      MAP_PHYSICAL, -1, 0);
    }

    struct range {
        void *start;
        size_t len;
        bool (*search_fn)(void *start, size_t len);
    } const search_data[] = {
#if ENABLE_ACPI
        range{ p_ebda, 0x400, acpi_find_rsdp },
        range{ p_e0000, 0x20000, acpi_find_rsdp },
#endif
        range{ p_ebda, 0x400, mp_find_fps },
        range{ p_9fc00 != p_ebda ? p_9fc00 : nullptr, 0x400, mp_find_fps },
        range{ p_f0000, 0x10000, mp_find_fps }
    };

    for (size_t i = 0; i < countof(search_data); ++i) {
        if (unlikely(!search_data[i].start))
            continue;
        if (search_data[i].search_fn(
                    search_data[i].start, search_data[i].len))
            break;
    }

    if (acpi_rsdt_addr)
        acpi_parse_rsdt();
    else if (mp_tables)
        mp_parse_fps();

    munmap(p_9fc00, 0x400);
    munmap(p_e0000, 0x20000);
    if (p_ebda != p_9fc00)
        munmap(p_ebda, 0x400);

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

    apic_send_command(dest, APIC_CMD_VECTOR_n(intr) | dest_type | dest_mode);
}

void apic_eoi(int intr)
{
    apic->write32(APIC_REG_EOI, intr & 0);
}

static void apic_online(int enabled, int spurious_intr, int err_intr)
{
    uint32_t sir = apic->read32(APIC_REG_SIR);

    if (enabled)
        sir |= APIC_SIR_APIC_ENABLE;
    else
        sir &= ~APIC_SIR_APIC_ENABLE;

    if (spurious_intr >= 32)
        sir = (sir & -256) | spurious_intr;

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
    for (int i = 0; i < 64; i += 4) {
        printdbg("ap=%d APIC: ", ap);
        for (int x = 0; x < 4; ++x) {
            if (apic->reg_readable(i + x)) {
                printdbg("%s[%3x]=%08x%s",
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
    APIC_TRACE("Logical destination register value: 0x%x\n",
             apic->read32(APIC_REG_LDR));
#endif
}

static void apic_calibrate();

static void apic_configure_timer(
        uint32_t dcr, uint32_t icr, uint8_t timer_mode,
        uint8_t intr, bool mask = false)
{
    APIC_TRACE("configuring timer,"
               " dcr=0x%x, icr=0x%x, mode=0x%x, intr=0x%x, mask=%d\n",
               dcr, icr, timer_mode, intr, mask);
    apic->write32(APIC_REG_LVT_DCR, dcr);
    apic->write32(APIC_REG_LVT_TR, APIC_LVT_VECTOR_n(intr) |
                  APIC_LVT_TR_MODE_n(timer_mode) |
                  (mask ? APIC_LVT_MASK : 0));
    apic->write32(APIC_REG_LVT_ICR, icr);
}

int apic_init(int ap)
{
    uint64_t apic_base_msr = cpu_msr_get(APIC_BASE_MSR);

    if (!apic_base)
        apic_base = apic_base_msr & APIC_BASE_ADDR;

    if (!(apic_base_msr & APIC_BASE_GENABLE)) {
        APIC_TRACE("APIC was globally disabled!"
                   " Enabling...\n");
    }

    if (cpuid_has_x2apic()) {
        APIC_TRACE("Using x2APIC\n");
        if (!ap)
            apic = &apic_x2;

        cpu_msr_set(APIC_BASE_MSR, apic_base_msr |
                APIC_BASE_GENABLE | APIC_BASE_X2ENABLE);
    } else {
        APIC_TRACE("Using xAPIC\n");

        if (!ap) {
            // Bootstrap CPU only
            assert(!apic_ptr);
            apic_ptr = (uint32_t *)mmap(
                        (void*)(apic_base),
                        4096, PROT_READ | PROT_WRITE,
                        MAP_PHYSICAL | MAP_NOCACHE |
                        MAP_WRITETHRU, -1, 0);

            apic = &apic_x;
        }

        cpu_msr_set(APIC_BASE_MSR,
                    (apic_base_msr & ~APIC_BASE_X2ENABLE) | APIC_BASE_GENABLE);
    }

    // Set global enable if it is clear
    if (!(apic_base_msr & APIC_BASE_GENABLE)) {
        APIC_TRACE("APIC was globally disabled!"
                   " Enabling...\n");
    }

    if (!ap) {
        intr_hook(INTR_APIC_TIMER, apic_timer_handler);
        intr_hook(INTR_APIC_SPURIOUS, apic_spurious_handler);
        intr_hook(INTR_APIC_ERROR, apic_err_handler);

        APIC_TRACE("Parsing boot tables\n");

        parse_mp_tables();

        APIC_TRACE("Calibrating APIC timer\n");

        apic_calibrate();
    }

    APIC_TRACE("Enabling APIC\n");

    apic_online(1, INTR_APIC_SPURIOUS, INTR_APIC_ERROR);

    apic->write32(APIC_REG_TPR, 0x0);

    assert(apic_base == (cpu_msr_get(APIC_BASE_MSR) & APIC_BASE_ADDR));

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

    topo_cpu_count = apic_id_count *
            topo_core_count * topo_thread_count;
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

    APIC_TRACE("%d CPU packages\n", apic_id_count);

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
        apic_send_command(0xFFFFFFFF,
                          APIC_CMD_DELIVERY_INIT |
                          APIC_CMD_DEST_MODE_LOGICAL |
                          APIC_CMD_DEST_TYPE_OTHER);
//        APIC_TRACE("Sending INIT IPI to APIC ID 0x%x\n", apic_id_list[i]);
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

        // The AP might have CD/NW bits set, and therefore might not be
        // cache coherent when it fetches the instructions in the trampoline,
        // so write all caches back to RAM before issuing any SIPIs
        cpu_flush_cache();

        for (unsigned thread = 0;
             thread < topo_thread_count; ++thread) {
            for (unsigned core = 0; core < topo_core_count; ++core) {
                uint8_t target = apic_id_list[pkg] +
                        (thread | (core << topo_thread_bits));

                // Don't try to start BSP
                if (target == apic_id_list[0])
                    continue;

                APIC_TRACE("Sending SIPI to APIC ID %u, "
                           "trampoline page=0x%x\n",
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

    // SMP online
    callout_call(callout_type_t::smp_online);

    ioapic_irq_cpu(0, 1);
}

uint32_t apic_timer_count(void)
{
    return apic->read32(APIC_REG_LVT_CCR);
}

//
// ACPI timer

class acpi_gas_accessor_t {
public:
    static acpi_gas_accessor_t *from_gas(acpi_gas_t const& gas);
    static acpi_gas_accessor_t *from_ioport(uint16_t ioport, int size);

    virtual ~acpi_gas_accessor_t() {}
    virtual size_t get_size() const = 0;
    virtual int64_t read() const = 0;
    virtual void write(int64_t value) const = 0;
};

template<int size>
class acpi_gas_accessor_sysmem_t : public acpi_gas_accessor_t {
public:
    typedef typename type_from_size<size, true>::type value_type;

    acpi_gas_accessor_sysmem_t(uint64_t mem_addr)
    {
        mem = (value_type*)mmap((void*)mem_addr, size,
                                PROT_READ | PROT_WRITE,
                                MAP_PHYSICAL, -1, 0);
    }

    size_t get_size() const override final { return size; }

    int64_t read() const override final { return *mem; }

    void write(int64_t value) const override final
    {
        *mem = value_type(value);
    }

private:
    value_type *mem;
};

template<int size>
class acpi_gas_accessor_sysio_t : public acpi_gas_accessor_t {
public:
    typedef typename type_from_size<size, true>::type value_type;

    acpi_gas_accessor_sysio_t(uint64_t io_port)
        : port(ioport_t(io_port)) {}

    size_t get_size() const override final { return size; }

    int64_t read() const override final { return inp<size>(port); }

    void write(int64_t value) const override final
    {
        outp<size>(port, value_type(value));
    }

private:
    ioport_t port;
};

template<int size>
class acpi_gas_accessor_pcicfg_t : public acpi_gas_accessor_t {
public:
    typedef typename type_from_size<size, true>::type value_type;

    acpi_gas_accessor_pcicfg_t(uint64_t pci_dfo)
        : dfo(pci_dfo) {}

    size_t get_size() const override final { return size; }

    int64_t read() const override final
    {
        value_type result;
        pci_config_copy(0, (dfo >> 32) & 0xFF,
                        (dfo >> 16) & 0xFF, &result,
                        dfo & 0xFF, size);
        return result;
    }

    void write(int64_t value) const override final
    {
        pci_config_write(0, (dfo >> 32) & 0xFF,
                         (dfo >> 16) & 0xFF, dfo & 0xFF, &value, size);
    }

private:
    uint64_t dfo;
};

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
            ACPI_TRACE("PM Timer at I/O port 0x%x\n", acpi_fadt.pm_timer_block);
            accessor = new acpi_gas_accessor_sysio_t<4>(
                        acpi_fadt.pm_timer_block);
        } else if (acpi_fadt.x_pm_timer_block.access_size) {
            accessor = acpi_gas_accessor_t::from_gas(
                        acpi_fadt.x_pm_timer_block);
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

__used
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
    if (b)
        return gcd(b, a % b);
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
    if (acpi_pm_timer_raw() >= 0) {
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
        } while (tmr_diff < 3579);

        uint32_t ccr_en = apic->read32(APIC_REG_LVT_CCR);
        uint64_t tsc_en = cpu_rdtsc();

        uint64_t tsc_elap = tsc_en - tsc_st;
        uint32_t ccr_elap = ccr_st - ccr_en;
        uint64_t tmr_nsec = acpi_pm_timer_ns(tmr_diff);

        uint64_t cpu_freq = (uint64_t(tsc_elap) * 1000000000) / tmr_nsec;
        uint64_t ccr_freq = (uint64_t(ccr_elap) * 1000000000) / tmr_nsec;

        apic_timer_freq = ccr_freq;

//        // Round APIC frequency to nearest multiple of 1MHz
//        apic_timer_freq += 1790;
//        apic_timer_freq -= apic_timer_freq % 1790;

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
        uint64_t tmr_nsec = nsleep(1000000);

        uint32_t ccr_en = apic->read32(APIC_REG_LVT_CCR);
        uint64_t tsc_en = cpu_rdtsc();

        uint64_t tsc_elap = tsc_en - tsc_st;
        uint32_t ccr_elap = ccr_st - ccr_en;

        uint64_t cpu_freq = (uint64_t(tsc_elap) * 1000000000) / tmr_nsec;
        uint64_t ccr_freq = (uint64_t(ccr_elap) * 1000000000) / tmr_nsec;

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

    APIC_TRACE("CPU MHz GCD: %ld\n", clk_to_ns_gcd);

    clk_to_ns_numer = 1000 / clk_to_ns_gcd;
    clk_to_ns_denom = rdtsc_mhz / clk_to_ns_gcd;

    APIC_TRACE("clk_to_ns_numer: %ld\n", clk_to_ns_numer);
    APIC_TRACE("clk_to_ns_denom: %ld\n", clk_to_ns_denom);

    if (cpuid_has_inrdtsc()) {
        APIC_TRACE("Using RDTSC for precision timing\n");
        time_ns_set_handler(apic_rdtsc_time_ns_handler, nullptr, true);
        nsleep_set_handler(apic_rdtsc_nsleep_handler, nullptr, true);
    }

    APIC_TRACE("CPU clock: %luMHz\n", rdtsc_mhz);
    APIC_TRACE("APIC clock: %luHz\n", apic_timer_freq);
}

//
// IOAPIC

static uint32_t ioapic_read(
        mp_ioapic_t *ioapic, uint32_t reg, unique_lock<spinlock> const&)
{
    ioapic->ptr[IOAPIC_IOREGSEL] = reg;
    return ioapic->ptr[IOAPIC_IOREGWIN];
}

static void ioapic_write(
        mp_ioapic_t *ioapic, uint32_t reg, uint32_t value,
        unique_lock<spinlock> const&)
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
    return 0;
}

static void ioapic_reset(mp_ioapic_t *ioapic)
{
    unique_lock<spinlock> lock(ioapic->lock);

    // Fill entries in interrupt-to-ioapic lookup table
    fill_n(intr_to_ioapic + ioapic->base_intr,
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

static void ioapic_set_flags(mp_ioapic_t *ioapic,
                             uint8_t intin, uint16_t intr_flags)
{
    unique_lock<spinlock> lock(ioapic->lock);

    uint32_t reg = ioapic_read(ioapic, IOAPIC_RED_LO_n(intin), lock);

    switch (intr_flags & ACPI_MADT_ENT_IRQ_FLAGS_POLARITY) {
    default:
    case ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_n(
            ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVEHI):
        IOAPIC_REDLO_POLARITY_SET(reg, IOAPIC_REDLO_POLARITY_ACTIVEHI);
        break;
    case ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_n(
            ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVELO):
        IOAPIC_REDLO_POLARITY_SET(reg, IOAPIC_REDLO_POLARITY_ACTIVELO);
        break;
    }

    switch (intr_flags & ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER) {
    default:
        APIC_TRACE("MP: Unrecognized IRQ trigger type!"
                   " Guessing edge\n");
        // fall through...
    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(
            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_DEFAULT):
    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(
            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_EDGE):
        IOAPIC_REDLO_TRIGGER_SET(reg, IOAPIC_REDLO_TRIGGER_EDGE);
        break;

    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(
            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_LEVEL):
        IOAPIC_REDLO_TRIGGER_SET(reg, IOAPIC_REDLO_TRIGGER_LEVEL);
        break;

    }

    ioapic_write(ioapic, IOAPIC_RED_LO_n(intin), reg, lock);
}

//
//

isr_context_t *apic_dispatcher(int intr, isr_context_t *ctx)
{
    assert(intr >= 0);
    assert(intr < 256);

    isr_context_t *orig_ctx = ctx;

    uint8_t irq = intr_to_irq[intr];

    apic_eoi(intr);
    ctx = (isr_context_t*)irq_invoke(intr, irq, ctx);

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

    unique_lock<spinlock> lock(ioapic->lock);

    uint32_t ent = ioapic_read(ioapic, IOAPIC_RED_LO_n(intin), lock);

    if (unmask)
        ent &= ~IOAPIC_REDLO_MASKIRQ;
    else
        ent |= IOAPIC_REDLO_MASKIRQ;

    ioapic_write(ioapic, IOAPIC_RED_LO_n(intin), ent, lock);
}

static void ioapic_hook(int irq, intr_handler_t handler)
{
    int intr = irq_to_intr[irq];
    assert(intr >= 0);
    intr_hook(intr, handler);
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

// Pass negative cpu value to get highest CPU number
// Returns 0 on failure, 1 on success
bool ioapic_irq_cpu(int irq, int cpu)
{
    if (cpu < 0)
        return ioapic_count > 0;

    if (unsigned(cpu) >= apic_id_count)
        return false;

    int irq_intr = irq_to_intr[irq];
    int ioapic_index = intr_to_ioapic[irq_intr];
    mp_ioapic_t *ioapic = ioapic_list + ioapic_index;
    unique_lock<spinlock> lock(ioapic->lock);
    ioapic_write(ioapic, IOAPIC_RED_HI_n(irq_intr - ioapic->base_intr),
                 IOAPIC_REDHI_DEST_n(apic_id_list[cpu]), lock);
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
    irq_setcpu_set_handler(ioapic_irq_cpu);

    return 1;
}

//
// MSI IRQ

// Returns the starting IRQ number of allocated range
// Returns 0 for failure
int apic_msi_irq_alloc(msi_irq_mem_t *results, int count,
                       int cpu, bool distribute,
                       intr_handler_t handler)
{
    // Don't try to use MSI if there are no IOAPIC devices
    if (ioapic_count == 0)
        return 0;

    // If out of range starting CPU number, force to zero
    if (cpu < 0 || (unsigned)cpu >= apic_id_count)
        cpu = 0;

    uint8_t vector_base = ioapic_aligned_vectors(bit_log2(count));

    // See if we ran out of vectors
    if (vector_base == 0)
        return 0;

    for (int i = 0; i < count; ++i) {
        results[i].addr = (0xFEEU << 20) |
                (apic_id_list[cpu] << 12);
        results[i].data = (vector_base + i);

        irq_to_intr[vector_base + i - INTR_APIC_IRQ_BASE] = vector_base + i;
        intr_to_irq[vector_base + i] = vector_base + i - INTR_APIC_IRQ_BASE;

        ioapic_hook(vector_base + i - ioapic_msi_base_intr +
                 ioapic_msi_base_irq,
                 handler);

        if (distribute) {
            if (++cpu >= int(apic_id_count))
                cpu = 0;
        }

        // It will always be edge triggered
        // (!!activehi << 14) |
        // (!!level << 15);
    }

    return vector_base - INTR_APIC_IRQ_BASE;
}

acpi_gas_accessor_t *acpi_gas_accessor_t::from_gas(acpi_gas_t const& gas)
{
    uint64_t addr = gas.addr_lo | (uint64_t(gas.addr_hi) << 32);

    ACPI_TRACE("Using extended PM Timer Generic Address Structure: "
               " addr_space: 0x%x, addr=0x%lx, size=0x%x,"
               " width=0x%x, bit=0x%x\n",
               gas.addr_space, addr, gas.access_size,
               gas.bit_width, gas.bit_offset);

    switch (gas.addr_space) {
    case ACPI_GAS_ADDR_SYSMEM:
        ACPI_TRACE("ACPI PM Timer using MMIO address space: 0x%lx\n", addr);
        switch (gas.access_size) {
        case 1: return new acpi_gas_accessor_sysmem_t<1>(addr);
        case 2: return new acpi_gas_accessor_sysmem_t<2>(addr);
        case 4: return new acpi_gas_accessor_sysmem_t<4>(addr);
        case 8: return new acpi_gas_accessor_sysmem_t<8>(addr);
        default: return nullptr;
        }
    case ACPI_GAS_ADDR_SYSIO:
        ACPI_TRACE("ACPI PM Timer using I/O address space: 0x%lx\n", addr);
        switch (gas.access_size) {
        case 1: return new acpi_gas_accessor_sysio_t<1>(addr);
        case 2: return new acpi_gas_accessor_sysio_t<2>(addr);
        case 4: return new acpi_gas_accessor_sysio_t<4>(addr);
        case 8: return nullptr;
        default: return nullptr;
        }
    case ACPI_GAS_ADDR_PCICFG:
        ACPI_TRACE("ACPI PM Timer using PCI config address space: 0x%lx\n",
                   addr);
        switch (gas.access_size) {
        case 1: return new acpi_gas_accessor_pcicfg_t<1>(addr);
        case 2: return new acpi_gas_accessor_pcicfg_t<2>(addr);
        case 4: return new acpi_gas_accessor_pcicfg_t<4>(addr);
        case 8: return new acpi_gas_accessor_pcicfg_t<8>(addr);
        default: return nullptr;
        }
    default:
        ACPI_TRACE("Unhandled ACPI PM Timer address space: 0x%x\n",
                   gas.addr_space);
        return nullptr;
    }
}
