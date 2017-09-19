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

#define ENABLE_ACPI 1

#define DEBUG_ACPI  1
#if DEBUG_ACPI
#define ACPI_TRACE(...) printdbg("acpi: " __VA_ARGS__)
#else
#define ACPI_TRACE(...) ((void)0)
#endif

#define DEBUG_APIC  1
#if DEBUG_APIC
#define APIC_TRACE(...) printdbg("lapic: " __VA_ARGS__)
#else
#define APIC_TRACE(...) ((void)0)
#endif

#define DEBUG_IOAPIC  1
#if DEBUG_IOAPIC
#define IOAPIC_TRACE(...) printdbg("ioapic: " __VA_ARGS__)
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

#define MP_CPU_FLAGS_ENABLED_BIT    0
#define MP_CPU_FLAGS_BSP_BIT        1

#define MP_CPU_FLAGS_ENABLED        (1<<MP_CPU_FLAGS_ENABLED_BIT)
#define MP_CPU_FLAGS_BSP            (1<<MP_CPU_FLAGS_BSP_BIT)

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

#define MP_IOAPIC_FLAGS_ENABLED_BIT   0

#define MP_IOAPIC_FLAGS_ENABLED       (1<<MP_IOAPIC_FLAGS_ENABLED_BIT)

// entry_type 3 and 4 flags

#define MP_INTR_FLAGS_POLARITY_BIT    0
#define MP_INTR_FLAGS_POLARITY_BITS   2
#define MP_INTR_FLAGS_TRIGGER_BIT     2
#define MP_INTR_FLAGS_TRIGGER_BITS    2

#define MP_INTR_FLAGS_TRIGGER_MASK \
        ((1<<MP_INTR_FLAGS_TRIGGER_BITS)-1)

#define MP_INTR_FLAGS_POLARITY_MASK   \
        ((1<<MP_INTR_FLAGS_POLARITY_BITS)-1)

#define MP_INTR_FLAGS_TRIGGER \
        (MP_INTR_FLAGS_TRIGGER_MASK<<MP_INTR_FLAGS_TRIGGER_BITS)

#define MP_INTR_FLAGS_POLARITY \
        (MP_INTR_FLAGS_POLARITY_MASK << \
        MP_INTR_FLAGS_POLARITY_BITS)

#define MP_INTR_FLAGS_POLARITY_DEFAULT    0
#define MP_INTR_FLAGS_POLARITY_ACTIVEHI   1
#define MP_INTR_FLAGS_POLARITY_ACTIVELO   3

#define MP_INTR_FLAGS_TRIGGER_DEFAULT     0
#define MP_INTR_FLAGS_TRIGGER_EDGE        1
#define MP_INTR_FLAGS_TRIGGER_LEVEL       3

#define MP_INTR_FLAGS_POLARITY_n(n)   ((n)<<MP_INTR_FLAGS_POLARITY_BIT)
#define MP_INTR_FLAGS_TRIGGER_n(n)    ((n)<<MP_INTR_FLAGS_TRIGGER_BIT)

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

#define IOAPIC_VER_VERSION_BIT  0
#define IOAPIC_VER_VERSION_BITS 8
#define IOAPIC_VER_VERSION_MASK ((1<<IOAPIC_VER_VERSION_BITS)-1)
#define IOAPIC_VER_VERSION      \
    (IOAPIC_VER_VERSION_MASK<<IOAPIC_VER_VERSION_BIT)
#define IOAPIC_VER_VERSION_n(n) ((n)<<IOAPIC_VER_VERSION_BIT)

#define IOAPIC_VER_ENTRIES_BIT  16
#define IOAPIC_VER_ENTRIES_BITS 8
#define IOAPIC_VER_ENTRIES_MASK ((1<<IOAPIC_VER_ENTRIES_BITS)-1)
#define IOAPIC_VER_ENTRIES      \
    (IOAPIC_VER_ENTRIES_MASK<<IOAPIC_VER_ENTRIES_BIT)
#define IOAPIC_VER_ENTRIES_n(n) ((n)<<IOAPIC_VER_ENTRIES_BIT)

#define IOAPIC_RED_LO_n(n)      (0x10 + (n) * 2)
#define IOAPIC_RED_HI_n(n)      (0x10 + (n) * 2 + 1)

#define IOAPIC_REDLO_VECTOR_BIT     0
#define IOAPIC_REDLO_DELIVERY_BIT   8
#define IOAPIC_REDLO_DESTMODE_BIT   11
#define IOAPIC_REDLO_STATUS_BIT     12
#define IOAPIC_REDLO_POLARITY_BIT   13
#define IOAPIC_REDLO_REMOTEIRR_BIT  14
#define IOAPIC_REDLO_TRIGGER_BIT    15
#define IOAPIC_REDLO_MASKIRQ_BIT    16
#define IOAPIC_REDHI_DEST_BIT       (56-32)

#define IOAPIC_REDLO_DESTMODE       (1<<IOAPIC_REDLO_DESTMODE_BIT)
#define IOAPIC_REDLO_STATUS         (1<<IOAPIC_REDLO_STATUS_BIT)
#define IOAPIC_REDLO_POLARITY       (1<<IOAPIC_REDLO_POLARITY_BIT)
#define IOAPIC_REDLO_REMOTEIRR      (1<<IOAPIC_REDLO_REMOTEIRR_BIT)
#define IOAPIC_REDLO_TRIGGER        (1<<IOAPIC_REDLO_TRIGGER_BIT)
#define IOAPIC_REDLO_MASKIRQ        (1<<IOAPIC_REDLO_MASKIRQ_BIT)

#define IOAPIC_REDLO_VECTOR_BITS    8
#define IOAPIC_REDLO_DELVERY_BITS   3
#define IOAPIC_REDHI_DEST_BITS      8

#define IOAPIC_REDLO_VECTOR_MASK    ((1<<IOAPIC_REDLO_VECTOR_BITS)-1)
#define IOAPIC_REDLO_DELVERY_MASK   ((1<<IOAPIC_REDLO_DELVERY_BITS)-1)
#define IOAPIC_REDHI_DEST_MASK      ((1<<IOAPIC_REDHI_DEST_BITS)-1)

#define IOAPIC_REDLO_VECTOR \
    (IOAPIC_REDLO_VECTOR_MASK<<IOAPIC_REDLO_VECTOR_BITS)
#define IOAPIC_REDLO_DELVERY \
    (IOAPIC_REDLO_DELVERY_MASK<<IOAPIC_REDLO_DELVERY_BITS)
#define IOAPIC_REDHI_DEST \
    (IOAPIC_REDHI_DEST_MASK<<IOAPIC_REDHI_DEST_BITS)

#define IOAPIC_REDLO_VECTOR_n(n)    ((n)<<IOAPIC_REDLO_VECTOR_BIT)
#define IOAPIC_REDLO_DELIVERY_n(n)  ((n)<<IOAPIC_REDLO_DELIVERY_BIT)
#define IOAPIC_REDLO_TRIGGER_n(n)   ((n)<<IOAPIC_REDLO_TRIGGER_BIT)
#define IOAPIC_REDHI_DEST_n(n)      ((n)<<IOAPIC_REDHI_DEST_BIT)
#define IOAPIC_REDLO_POLARITY_n(n)  ((n)<<IOAPIC_REDLO_POLARITY_BIT)

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

struct mp_bus_irq_mapping_t {
    uint8_t bus;
    uint8_t intr_type;
    uint8_t flags;
    uint8_t device;
    uint8_t irq;
    uint8_t ioapic_id;
    uint8_t intin;
};

struct mp_ioapic_t {
    uint8_t id;
    uint8_t base_intr;
    uint8_t vector_count;
    uint8_t irq_base;
    uint32_t addr;
    uint32_t volatile *ptr;
    spinlock_t lock;
};

static char *mp_tables;

static vector<uint8_t> mp_pci_bus_ids;
static uint16_t mp_isa_bus_id;

static vector<mp_bus_irq_mapping_t> bus_irq_list;

static uint8_t bus_irq_to_mapping[64];

static uint64_t apic_timer_freq;
static uint64_t rdtsc_mhz;
static uint64_t clk_to_ns_numer;
static uint64_t clk_to_ns_denom;

static mp_ioapic_t ioapic_list[16];
static unsigned ioapic_count;

static spinlock_t ioapic_msi_alloc_lock;
static uint8_t ioapic_msi_next_irq = INTR_APIC_IRQ_BASE;
static uint64_t ioapic_msi_alloc_map[] = {
    0x0000000000000000L,
    0x0000000000000000L,
    0x0000000000000000L,
    0x8000000000000000L
};

static uint8_t ioapic_next_irq_base = 16;

// First vector after last IOAPIC
static uint8_t ioapic_msi_base_intr;
static uint8_t ioapic_msi_base_irq;

static uint8_t isa_irq_lookup[16];

static uint8_t apic_id_list[64];
static unsigned apic_id_count;

static uint8_t topo_thread_bits;
static uint8_t topo_thread_count;
static uint8_t topo_core_bits;
static uint8_t topo_core_count;

static uint8_t topo_cpu_count;

static uintptr_t apic_base;
static uint32_t volatile *apic_ptr;

#define MP_TABLE_TYPE_CPU       0
#define MP_TABLE_TYPE_BUS       1
#define MP_TABLE_TYPE_IOAPIC    2
#define MP_TABLE_TYPE_IOINTR    3
#define MP_TABLE_TYPE_LINTR     4
#define MP_TABLE_TYPE_ADDRMAP   128
#define MP_TABLE_TYPE_BUSHIER   129
#define MP_TABLE_TYPE_BUSCOMPAT 130

//
// APIC

// APIC ID (read only)
#define APIC_REG_ID             0x02

// APIC version (read only)
#define APIC_REG_VER            0x03

// Task Priority Register
#define APIC_REG_TPR            0x08

// Arbitration Priority Register (not present in x2APIC mode)
#define APIC_REG_APR            0x09

// Processor Priority Register (read only)
#define APIC_REG_PPR            0x0A

// End Of Interrupt register (must write 0 in x2APIC mode)
#define APIC_REG_EOI            0x0B

// Logical Destination Register (not writeable in x2APIC mode)
#define APIC_REG_LDR            0x0D

// Destination Format Register (not present in x2APIC mode)
#define APIC_REG_DFR            0x0E

// Spurious Interrupt Register
#define APIC_REG_SIR            0x0F

// In Service Registers (bit n) (read only)
#define APIC_REG_ISR_n(n)       (0x10 + ((n) >> 5))

// Trigger Mode Registers (bit n) (read only)
#define APIC_REG_TMR_n(n)       (0x18 + ((n) >> 5))

// Interrupt Request Registers (bit n) (read only)
#define APIC_REG_IRR_n(n)       (0x20 + ((n) >> 5))

// Error Status Register (not present in x2APIC mode)
#define APIC_REG_ESR            0x28

// Local Vector Table Corrected Machine Check Interrupt
#define APIC_REG_LVT_CMCI       0x2F

// Local Vector Table Interrupt Command Register Lo (64-bit in x2APIC mode)
#define APIC_REG_ICR_LO         0x30

// Local Vector Table Interrupt Command Register Hi (not present in x2APIC mode)
#define APIC_REG_ICR_HI         0x31

// Local Vector Table Timer Register
#define APIC_REG_LVT_TR         0x32

// Local Vector Table Thermal Sensor Register
#define APIC_REG_LVT_TSR        0x33

// Local Vector Table Performance Monitoring Counter Register
#define APIC_REG_LVT_PMCR       0x34

// Local Vector Table Local Interrupt 0 Register
#define APIC_REG_LVT_LNT0       0x35

// Local Vector Table Local Interrupt 1 Register
#define APIC_REG_LVT_LNT1       0x36

// Local Vector Table Error Register
#define APIC_REG_LVT_ERR        0x37

// Local Vector Table Timer Initial Count Register
#define APIC_REG_LVT_ICR        0x38

// Local Vector Table Timer Current Count Register (read only)
#define APIC_REG_LVT_CCR        0x39

// Local Vector Table Timer Divide Configuration Register
#define APIC_REG_LVT_DCR        0x3E

// Self Interprocessor Interrupt (x2APIC only, write only)
#define APIC_REG_SELF_IPI       0x3F

// Only used in xAPIC mode
#define APIC_DEST_BIT               24
#define APIC_DEST_BITS              8
#define APIC_DEST_MASK              ((1U << APIC_DEST_BITS)-1U)
#define APIC_DEST_n(n)              \
    (((n) & APIC_DEST_MASK) << APIC_DEST_BIT)

#define APIC_CMD_SIPI_PAGE_BIT      0
#define APIC_CMD_VECTOR_BIT         0
#define APIC_CMD_DEST_MODE_BIT      8
#define APIC_CMD_DEST_LOGICAL_BIT   11
#define APIC_CMD_PENDING_BIT        12
#define APIC_CMD_ILD_CLR_BIT        14
#define APIC_CMD_ILD_SET_BIT        15
#define APIC_CMD_DEST_TYPE_BIT      18

#define APIC_CMD_VECTOR_BITS        8
#define APIC_CMD_SIPI_PAGE_BITS     8
#define APIC_CMD_DEST_MODE_BITS     3
#define APIC_CMD_DEST_TYPE_BITS     2

#define APIC_CMD_VECTOR_MASK        ((1 << APIC_CMD_VECTOR_BITS)-1)
#define APIC_CMD_SIPI_PAGE_MASK     ((1 << APIC_CMD_SIPI_PAGE_BITS)-1)
#define APIC_CMD_DEST_MODE_MASK     ((1 << APIC_CMD_DEST_MODE_BITS)-1)
#define APIC_CMD_DEST_TYPE_MASK     ((1 << APIC_CMD_DEST_TYPE_BITS)-1)

#define APIC_CMD_SIPI_PAGE      \
    (APIC_CMD_SIPI_PAGE_MASK << APIC_CMD_SIPI_PAGE_BIT)
#define APIC_CMD_DEST_MODE      \
    (APIC_CMD_DEST_MODE_MASK << APIC_CMD_DEST_MODE_BIT)
#define APIC_CMD_SIPI_PAGE      \
    (APIC_CMD_SIPI_PAGE_MASK << APIC_CMD_SIPI_PAGE_BIT)

#define APIC_CMD_SIPI_PAGE_n(n) ((n) << APIC_CMD_SIPI_PAGE_BIT)
#define APIC_CMD_DEST_MODE_n(n) ((n) << APIC_CMD_DEST_MODE_BIT)
#define APIC_CMD_DEST_TYPE_n(n) ((n) << APIC_CMD_DEST_TYPE_BIT)
#define APIC_CMD_SIPI_PAGE_n(n) ((n) << APIC_CMD_SIPI_PAGE_BIT)

#define APIC_CMD_VECTOR_n(n)    \
    (((n) & APIC_CMD_VECTOR_MASK) << APIC_CMD_VECTOR_BIT)

#define APIC_CMD_VECTOR         (1U << APIC_CMD_VECTOR_BIT)
#define APIC_CMD_DEST_LOGICAL   (1U << APIC_CMD_DEST_LOGICAL_BIT)
#define APIC_CMD_PENDING        (1U << APIC_CMD_PENDING_BIT)
#define APIC_CMD_ILD_CLR        (1U << APIC_CMD_ILD_CLR_BIT)
#define APIC_CMD_ILD_SET        (1U << APIC_CMD_ILD_SET_BIT)
#define APIC_CMD_DEST_TYPE      (1U << APIC_CMD_DEST_TYPE_BIT)

#define APIC_CMD_DEST_MODE_NORMAL   APIC_CMD_DEST_MODE_n(0)
#define APIC_CMD_DEST_MODE_LOWPRI   APIC_CMD_DEST_MODE_n(1)
#define APIC_CMD_DEST_MODE_SMI      APIC_CMD_DEST_MODE_n(2)
#define APIC_CMD_DEST_MODE_NMI      APIC_CMD_DEST_MODE_n(4)
#define APIC_CMD_DEST_MODE_INIT     APIC_CMD_DEST_MODE_n(5)
#define APIC_CMD_DEST_MODE_SIPI     APIC_CMD_DEST_MODE_n(6)

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

#define APIC_LVT_TR_MODE_BIT        17
#define APIC_LVT_TR_MODE_BITS       2
#define APIC_LVT_TR_MODE_MASK       ((1U<<APIC_LVT_TR_MODE_BITS)-1U)
#define APIC_LVT_TR_MODE_n(n)       ((n)<<APIC_LVT_TR_MODE_BIT)

#define APIC_LVT_TR_MODE_ONESHOT    0
#define APIC_LVT_TR_MODE_PERIODIC   1
#define APIC_LVT_TR_MODE_DEADLINE   2

#define APIC_LVT_MASK_BIT       16
#define APIC_LVT_PENDING_BIT    12
#define APIC_LVT_LEVEL_BIT      15
#define APIC_LVT_REMOTEIRR_BIT  14
#define APIC_LVT_ACTIVELOW_BIT  13
#define APIC_LVT_DELIVERY_BIT   8
#define APIC_LVT_DELIVERY_BITS  3

#define APIC_LVT_MASK           (1U<<APIC_LVT_MASK_BIT)
#define APIC_LVT_PENDING        (1U<<APIC_LVT_PENDING_BIT)
#define APIC_LVT_LEVEL          (1U<<APIC_LVT_LEVEL_BIT)
#define APIC_LVT_REMOTEIRR      (1U<<APIC_LVT_REMOTEIRR_BIT)
#define APIC_LVT_ACTIVELOW      (1U<<APIC_LVT_ACTIVELOW_BIT)
#define APIC_LVT_DELIVERY_MASK  ((1U<<APIC_LVT_DELIVERY_BITS)-1)
#define APIC_LVT_DELIVERY_n(n)  ((n)<<APIC_LVT_DELIVERY_BIT)

#define APIC_LVT_DELIVERY_FIXED 0
#define APIC_LVT_DELIVERY_SMI   2
#define APIC_LVT_DELIVERY_NMI   4
#define APIC_LVT_DELIVERY_EXINT 7
#define APIC_LVT_DELIVERY_INIT  5

#define APIC_LVT_VECTOR_BIT     0
#define APIC_LVT_VECTOR_BITS    8
#define APIC_LVT_VECTOR_MASK    ((1U<<APIC_LVT_VECTOR_BITS)-1U)
#define APIC_LVT_VECTOR_n(n)    ((n)<<APIC_LVT_VECTOR_BIT)

#define APIC_BASE_MSR  0x1B

#define APIC_BASE_ADDR_BIT      12
#define APIC_BASE_ADDR_BITS     40
#define APIC_BASE_GENABLE_BIT   11
#define APIC_BASE_X2ENABLE_BIT  10
#define APIC_BASE_BSP_BIT       8

#define APIC_BASE_GENABLE       (1UL<<APIC_BASE_GENABLE_BIT)
#define APIC_BASE_X2ENABLE      (1UL<<APIC_BASE_X2ENABLE_BIT)
#define APIC_BASE_BSP           (1UL<<APIC_BASE_BSP_BIT)
#define APIC_BASE_ADDR_MASK     ((1UL<<APIC_BASE_ADDR_BITS)-1)
#define APIC_BASE_ADDR          (APIC_BASE_ADDR_MASK<<APIC_BASE_ADDR_BIT)

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
    void command(uint32_t dest, uint32_t cmd) const final
    {
        cpu_scoped_irq_disable intr_enabled;
        write32(APIC_REG_ICR_HI, APIC_DEST_n(dest));
        write32(APIC_REG_ICR_LO, cmd);
        intr_enabled.restore();
        while (read32(APIC_REG_ICR_LO) & APIC_CMD_PENDING)
            pause();
    }

    uint32_t read32(uint32_t offset) const final
    {
        return apic_ptr[offset << (4 - 2)];
    }

    void write32(uint32_t offset, uint32_t val) const final
    {
        apic_ptr[offset << (4 - 2)] = val;
    }

    uint64_t read64(uint32_t offset) const final
    {
        return ((uint64_t*)apic_ptr)[offset << (4 - 3)];
    }

    void write64(uint32_t offset, uint64_t val) const final
    {
        ((uint64_t*)apic_ptr)[offset << (4 - 3)] = val;
    }

    bool reg_readable(uint32_t reg) const final
    {
        return reg_maybe_readable(reg);
    }

    bool reg_exists(uint32_t reg) const final
    {
        return reg_maybe_readable(reg);
    }
};

class lapic_x2_t : public lapic_t {
    void command(uint32_t dest, uint32_t cmd) const final
    {
        write64(APIC_REG_ICR_LO, (uint64_t(dest) << 32) | cmd);
    }

    uint32_t read32(uint32_t offset) const final
    {
        return msr_get_lo(0x800 + offset);
    }

    void write32(uint32_t offset, uint32_t val) const final
    {
        msr_set(0x800 + offset, val);
    }

    uint64_t read64(uint32_t offset) const final
    {
        return msr_get(0x800 + offset);
    }

    void write64(uint32_t offset, uint64_t val) const final
    {
        msr_set(0x800 + offset, val);
    }

    bool reg_readable(uint32_t reg) const final
    {
        // APIC_REG_LVT_CMCI raises #GP if CMCI not enabled
        return reg != APIC_REG_LVT_CMCI &&
                reg != APIC_REG_ICR_HI &&
                reg_exists(reg) &&
                reg_maybe_readable(reg);
    }

    bool reg_exists(uint32_t reg) const final
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

//
// FADT Fixed Feature Flags

#define ACPI_FADT_FFF_WBINVD_BIT                0
#define ACPI_FADT_FFF_WBINVD_FLUSH_BIT          1
#define ACPI_FADT_FFF_PROC_C1_BIT               2
#define ACPI_FADT_FFF_P_LVL2_MP_BIT             3
#define ACPI_FADT_FFF_PWR_BUTTON_BIT            4
#define ACPI_FADT_FFF_SLP_BUTTON_BIT            5
#define ACPI_FADT_FFF_FIX_RTC_BIT               6
#define ACPI_FADT_FFF_RTC_S4_BIT                7
#define ACPI_FADT_FFF_TMR_VAL_EXT_BIT           8
#define ACPI_FADT_FFF_DCK_CAP_BIT               9
#define ACPI_FADT_FFF_RESET_REG_SUP_BIT         10
#define ACPI_FADT_FFF_SEALED_CASE_BIT           11
#define ACPI_FADT_FFF_HEADLESS_BIT              12
#define ACPI_FADT_FFF_CPU_SW_SLP_BIT            13
#define ACPI_FADT_FFF_PCI_EXP_WAK_BIT           14
#define ACPI_FADT_FFF_PLAT_CLOCK_BIT            15
#define ACPI_FADT_FFF_S4_RTC_STS_BIT            16
#define ACPI_FADT_FFF_REMOTE_ON_CAP_BIT         17
#define ACPI_FADT_FFF_FORCE_CLUSTER_BIT         18
#define ACPI_FADT_FFF_FORCE_PHYS_DEST_BIT       19
#define ACPI_FADT_FFF_HW_REDUCED_ACPI_BIT       20
#define ACPI_FADT_FFF_LOCAL_POWER_S0_CAP_BIT    21

#define ACPI_FADT_FFF_WBINVD \
    (1U<<ACPI_FADT_FFF_WBINVD_BIT)
#define ACPI_FADT_FFF_WBINVD_FLUSH \
    (1U<<ACPI_FADT_FFF_WBINVD_FLUSH_BIT)
#define ACPI_FADT_FFF_PROC_C1 \
    (1U<<ACPI_FADT_FFF_PROC_C1_BIT)
#define ACPI_FADT_FFF_P_LVL2_MP \
    (1U<<ACPI_FADT_FFF_P_LVL2_MP_BIT)
#define ACPI_FADT_FFF_PWR_BUTTON \
    (1U<<ACPI_FADT_FFF_PWR_BUTTON_BIT)
#define ACPI_FADT_FFF_SLP_BUTTON \
    (1U<<ACPI_FADT_FFF_SLP_BUTTON_BIT)
#define ACPI_FADT_FFF_FIX_RTC \
    (1U<<ACPI_FADT_FFF_FIX_RTC_BIT)
#define ACPI_FADT_FFF_RTC_S4 \
    (1U<<ACPI_FADT_FFF_RTC_S4_BIT)
#define ACPI_FADT_FFF_TMR_VAL_EXT \
    (1U<<ACPI_FADT_FFF_TMR_VAL_EXT_BIT)
#define ACPI_FADT_FFF_DCK_CAP \
    (1U<<ACPI_FADT_FFF_DCK_CAP_BIT)
#define ACPI_FADT_FFF_RESET_REG_SUP \
    (1U<<ACPI_FADT_FFF_RESET_REG_SUP_BIT)
#define ACPI_FADT_FFF_SEALED_CASE \
    (1U<<ACPI_FADT_FFF_SEALED_CASE_BIT)
#define ACPI_FADT_FFF_HEADLESS \
    (1U<<ACPI_FADT_FFF_HEADLESS_BIT)
#define ACPI_FADT_FFF_CPU_SW_SLP \
    (1U<<ACPI_FADT_FFF_CPU_SW_SLP_BIT)
#define ACPI_FADT_FFF_PCI_EXP_WAK \
    (1U<<ACPI_FADT_FFF_PCI_EXP_WAK_BIT)
#define ACPI_FADT_FFF_PLAT_CLOCK \
    (1U<<ACPI_FADT_FFF_PLAT_CLOCK_BIT)
#define ACPI_FADT_FFF_S4_RTC_STS \
    (1U<<ACPI_FADT_FFF_S4_RTC_STS_BIT)
#define ACPI_FADT_FFF_REMOTE_ON_CAP \
    (1U<<ACPI_FADT_FFF_REMOTE_ON_CAP_BIT)
#define ACPI_FADT_FFF_FORCE_CLUSTER \
    (1U<<ACPI_FADT_FFF_FORCE_CLUSTER_BIT)
#define ACPI_FADT_FFF_FORCE_PHYS_DEST \
    (1U<<ACPI_FADT_FFF_FORCE_PHYS_DEST_BIT)
#define ACPI_FADT_FFF_HW_REDUCED_ACPI \
    (1U<<ACPI_FADT_FFF_HW_REDUCED_ACPI_BIT)
#define ACPI_FADT_FFF_LOCAL_POWER_S0_CAP \
    (1U<<ACPI_FADT_FFF_LOCAL_POWER_S0_CAP_BIT)


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

#define ACPI_MADT_FLAGS_HAVE_PIC_BIT    0
#define ACPI_MADT_FLAGS_HAVE_PIC        (1<<ACPI_MADT_FLAGS_HAVE_PIC_BIT)

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

#define ACPI_HPET_BLKID_PCI_VEN_MASK \
    ((1<<ACPI_HPET_BLKID_PCI_VEN_BITS)-1)
#define ACPI_HPET_BLKID_NUM_CMP_MASK \
    ((1<<ACPI_HPET_BLKID_NUM_CMP_BITS)-1)
#define ACPI_HPET_BLKID_REV_ID_MASK \
    ((1<<ACPI_HPET_BLKID_REV_ID_BITS)-1)

// LegacyReplacement IRQ Routing Capable
#define ACPI_HPET_BLKID_LEGACY_CAP \
    (1<<ACPI_HPET_BLKID_LEGACY_CAP_BIT)

// 64-bit counters
#define ACPI_HPET_BLKID_COUNTER_SZ \
    (1<<ACPI_HPET_BLKID_COUNTER_SZ_BIT)

// PCI Vendor ID
#define ACPI_HPET_BLKID_PCI_VEN \
    (ACPI_HPET_BLKID_PCI_VEN_MASK<<ACPI_HPET_BLKID_PCI_VEN_BIT)

// Number of comparators in 1st block
#define ACPI_HPET_BLKID_NUM_CMP \
    (ACPI_HPET_BLKID_NUM_CMP_MASK<<ACPI_HPET_BLKID_NUM_CMP_BIT)

// Hardware revision ID
#define ACPI_HPET_BLKID_REV_ID \
    (ACPI_HPET_BLKID_REV_ID_BITS<<ACPI_HPET_BLKID_REV_ID_BIT)

static vector<acpi_gas_t> acpi_hpet_list;
static int acpi_madt_flags;

static acpi_fadt_t acpi_fadt;

// The ACPI PM timer runs at 3.579545MHz
#define ACPI_PM_TIMER_HZ    3579545

static uint64_t acpi_rsdp_addr;

int acpi_have8259pic(void)
{
    return !acpi_rsdp_addr ||
            !!(acpi_madt_flags & ACPI_MADT_FLAGS_HAVE_PIC);
}

static uint8_t ioapic_alloc_vectors(uint8_t count)
{
    spinlock_lock_noirq(&ioapic_msi_alloc_lock);

    uint8_t base = ioapic_msi_next_irq;

    for (size_t intr = base - INTR_APIC_IRQ_BASE, end = intr + count;
         intr < end; ++intr) {
        ioapic_msi_alloc_map[intr >> 6] |= (1UL << (intr & 0x3F));
    }

    spinlock_unlock_noirq(&ioapic_msi_alloc_lock);

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

    spinlock_lock_noirq(&ioapic_msi_alloc_lock);

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

    spinlock_unlock_noirq(&ioapic_msi_alloc_lock);

    return result;
}

static uint8_t checksum_bytes(char const *bytes, size_t len)
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
        switch (ent->hdr.entry_type) {
        case ACPI_MADT_REC_TYPE_LAPIC:
            if (apic_id_count < countof(apic_id_list)) {
                // If processor is enabled
                if (ent->lapic.flags == 1)
                    apic_id_list[apic_id_count++] = ent->lapic.apic_id;
                else
                    printdbg("Disabled processor detected\n");
            } else {
                printdbg("Too many CPU packages! Dropped one\n");
            }
            break;

        case ACPI_MADT_REC_TYPE_IOAPIC:
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
            }
            break;

        case ACPI_MADT_REC_TYPE_IRQ:
            if (bus_irq_list.empty()) {
                // Populate bus_irq_list with 16 legacy ISA IRQs
                for (int i = 0; i < 16; ++i) {
                    mp_bus_irq_mapping_t mapping{};
                    mapping.bus = ent->irq_src.bus;
                    mapping.intin = i;
                    mapping.irq = i;
                    mapping.flags = ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVEHI |
                            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_EDGE;
                    isa_irq_lookup[i] = i;
                    bus_irq_list.push_back(mapping);
                }
            }

            mp_bus_irq_mapping_t &mapping = bus_irq_list[ent->irq_src.gsi];
            mapping.bus = ent->irq_src.bus;
            mapping.irq = ent->irq_src.irq_src;
            mapping.flags = ent->irq_src.flags;
            isa_irq_lookup[ent->irq_src.irq_src] = ent->irq_src.gsi;
            break;
        }
    }
}

static void acpi_process_hpet(acpi_hpet_t *acpi_hdr)
{
    acpi_hpet_list.push_back(acpi_hdr->addr);
}

static uint8_t acpi_chk_hdr(acpi_sdt_hdr_t *hdr)
{
    return checksum_bytes((char const *)hdr, hdr->len);
}

static int parse_mp_tables(void)
{
    void *mem_top =
            (uint16_t*)((uintptr_t)*BIOS_DATA_AREA(
                uint16_t, 0x40E) << 4);
    void *ranges[4] = {
        mem_top, (uint32_t*)0xA0000,
        0, 0
    };
    for (size_t pass = 0;
         !mp_tables && !acpi_rsdp_addr && pass < 4;
         pass += 2) {
        if (pass == 2) {
            ranges[2] = mmap((void*)0xE0000, 0x20000, PROT_READ,
                             MAP_PHYSICAL, -1, 0);
            ranges[3] = (char*)ranges[2] + 0x20000;
        }
        for (mp_table_hdr_t const* sig_srch =
             (mp_table_hdr_t const*)ranges[pass];
             (void*)sig_srch < ranges[pass+1]; ++sig_srch) {
#if ENABLE_ACPI
            // Check for ACPI 2.0+ RSDP
            acpi_rsdp2_t *rsdp2 = (acpi_rsdp2_t*)sig_srch;
            if (!memcmp(rsdp2->rsdp1.sig, "RSD PTR ", 8)) {
                // Check checksum
                if (rsdp2->rsdp1.rev != 0 &&
                        checksum_bytes((char*)rsdp2,
                                   sizeof(*rsdp2)) == 0 &&
                        checksum_bytes((char*)&rsdp2->rsdp1,
                                       sizeof(rsdp2->rsdp1)) == 0) {
                    if (rsdp2->xsdt_addr_lo | rsdp2->xsdt_addr_hi)
                        acpi_rsdp_addr = rsdp2->xsdt_addr_lo |
                                ((uint64_t)rsdp2->xsdt_addr_hi << 32);
                    else
                        acpi_rsdp_addr = rsdp2->rsdp1.rsdt_addr;
                    break;
                }
            }

            // Check for ACPI 1.0 RSDP
            acpi_rsdp_t *rsdp = (acpi_rsdp_t*)sig_srch;
            if (rsdp->rev == 0 &&
                    !memcmp(rsdp->sig, "RSD PTR ", 8)) {
                // Check checksum
                if (checksum_bytes((char*)rsdp, sizeof(*rsdp)) == 0) {
                    acpi_rsdp_addr = rsdp->rsdt_addr;
                    break;
                }
            }
#endif

            // Check for MP tables signature
            if (!memcmp(sig_srch->sig, "_MP_", 4)) {
                // Check checksum
                if (checksum_bytes((char*)sig_srch,
                                   sizeof(*sig_srch)) == 0) {
                    mp_tables = (char*)(uintptr_t)sig_srch->phys_addr;
                    break;
                }
            }
        }
    }

    if (acpi_rsdp_addr) {
        acpi_sdt_hdr_t *rsdt_hdr = (acpi_sdt_hdr_t *)mmap(
                    (void*)acpi_rsdp_addr, 64 << 10,
                    PROT_READ,
                    MAP_PHYSICAL, -1, 0);

        if (acpi_chk_hdr(rsdt_hdr) != 0) {
            printdbg("ACPI RSDT checksum mismatch!\n");
            return 0;
        }

        uint32_t *rsdp_ptrs = (uint32_t *)(rsdt_hdr + 1);
        uint32_t *rsdp_end = (uint32_t *)((char*)rsdt_hdr + rsdt_hdr->len);

        for (uint32_t *rsdp_ptr = rsdp_ptrs;
             rsdp_ptr < rsdp_end; ++rsdp_ptr) {
            uint32_t hdr_addr = *rsdp_ptr;
            acpi_sdt_hdr_t *hdr = (acpi_sdt_hdr_t *)
                    mmap((void*)(uintptr_t)hdr_addr,
                         64 << 10, PROT_READ, MAP_PHYSICAL, -1, 0);

            if (!memcmp(hdr->sig, "FACP", 4)) {
                acpi_fadt_t *fadt_hdr = (acpi_fadt_t *)hdr;

                if (acpi_chk_hdr(&fadt_hdr->hdr) == 0) {
                    printdbg("ACPI FADT found\n");
                    acpi_process_fadt(fadt_hdr);
                } else {
                    printdbg("ACPI FADT checksum mismatch!\n");
                }
            } else if (!memcmp(hdr->sig, "APIC", 4)) {
                acpi_madt_t *madt_hdr = (acpi_madt_t *)hdr;

                if (acpi_chk_hdr(&madt_hdr->hdr) == 0) {
                    printdbg("ACPI MADT found\n");
                    acpi_process_madt(madt_hdr);
                } else {
                    printdbg("ACPI MADT checksum mismatch!\n");
                }
            } else if (!memcmp(hdr->sig, "HPET", 4)) {
                acpi_hpet_t *hpet_hdr = (acpi_hpet_t *)hdr;

                if (acpi_chk_hdr(&hpet_hdr->hdr) == 0) {
                    printdbg("ACPI HPET found\n");
                    acpi_process_hpet(hpet_hdr);
                } else {
                    printdbg("ACPI MADT checksum mismatch!\n");
                }
            } else {
                if (acpi_chk_hdr(hdr) == 0) {
                    printdbg("ACPI %4.4s ignored\n", hdr->sig);
                } else {
                    printdbg("ACPI %4.4s checksum mismatch!"
                           " (ignored anyway)\n", hdr->sig);
                }
            }

            munmap(hdr, 64 << 10);
        }
    }

    if (mp_tables) {
        mp_cfg_tbl_hdr_t *cth = (mp_cfg_tbl_hdr_t *)
                mmap(mp_tables, 0x10000,
                     PROT_READ, MAP_PHYSICAL, -1, 0);

        uint8_t *entry = (uint8_t*)(cth + 1);

        // Reset to impossible values
        mp_isa_bus_id = -1;

        // First slot reserved for BSP
        apic_id_count = 1;

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

                printdbg("CPU package found,"
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

                printdbg("%.*s bus found, id=%u\n",
                         (int)sizeof(entry_bus->type),
                         entry_bus->type, entry_bus->bus_id);

                if (!memcmp(entry_bus->type, "PCI   ", 6)) {
                    mp_pci_bus_ids.push_back(entry_bus->bus_id);
                } else if (!memcmp(entry_bus->type, "ISA   ", 6)) {
                    if (mp_isa_bus_id == 0xFFFF)
                        mp_isa_bus_id = entry_bus->bus_id;
                    else
                        printdbg("Too many ISA busses,"
                                 " only one supported\n");
                } else {
                    printdbg("Dropped! Unrecognized bus named \"%.*s\"\n",
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
                        printdbg("Dropped! Too many IOAPIC devices\n");
                        break;
                    }

                    printdbg("IOAPIC id=%d, addr=0x%x,"
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
                                ioapic_intr_count);;

                    ioapic->lock = 0;
                }
                entry = (uint8_t*)(entry_ioapic + 1);
                break;
            }

            case MP_TABLE_TYPE_IOINTR:
            {
                entry_iointr = (mp_cfg_iointr_t *)entry;

                if (memchr(mp_pci_bus_ids.data(), entry_iointr->source_bus,
                            mp_pci_bus_ids.size())) {
                    // PCI IRQ
                    uint8_t bus = entry_iointr->source_bus;
                    uint8_t intr_type = entry_iointr->type;
                    uint8_t intr_flags = entry_iointr->flags;
                    uint8_t device = entry_iointr->source_bus_irq >> 2;
                    uint8_t pci_irq = entry_iointr->source_bus_irq & 3;
                    uint8_t ioapic_id = entry_iointr->dest_ioapic_id;
                    uint8_t intin = entry_iointr->dest_ioapic_intin;

                    printdbg("PCI device %u INT_%c# ->"
                             " IOAPIC ID 0x%02x INTIN %d %s\n",
                           device, (int)(pci_irq) + 'A',
                           ioapic_id, intin,
                             intr_type < countof(intr_type_text) ?
                                 intr_type_text[intr_type] :
                                 "(invalid type!)");


                    mp_bus_irq_mapping_t mapping{};
                    mapping.bus = bus;
                    mapping.intr_type = intr_type;
                    mapping.flags = intr_flags;
                    mapping.device = device;
                    mapping.irq = pci_irq & 3;
                    mapping.ioapic_id = ioapic_id;
                    mapping.intin = intin;
                    bus_irq_list.push_back(mapping);
                } else if (entry_iointr->source_bus == mp_isa_bus_id) {
                    // ISA IRQ

                    uint8_t bus =  entry_iointr->source_bus;
                    uint8_t intr_type = entry_iointr->type;
                    uint8_t intr_flags = entry_iointr->flags;
                    uint8_t isa_irq = entry_iointr->source_bus_irq;
                    uint8_t ioapic_id = entry_iointr->dest_ioapic_id;
                    uint8_t intin = entry_iointr->dest_ioapic_intin;

                    printdbg("ISA IRQ %d -> IOAPIC ID 0x%02x INTIN %u %s\n",
                           isa_irq, ioapic_id, intin,
                             intr_type < countof(intr_type_text) ?
                                 intr_type_text[intr_type] :
                                 "(invalid type!)");

                    isa_irq_lookup[isa_irq] = bus_irq_list.size();
                    mp_bus_irq_mapping_t mapping{};
                    mapping.bus = bus;
                    mapping.intr_type = intr_type;
                    mapping.flags = intr_flags;
                    mapping.device = 0;
                    mapping.irq = isa_irq;
                    mapping.ioapic_id = ioapic_id;
                    mapping.intin = intin;
                    bus_irq_list.push_back(mapping);
                } else {
                    // Unknown bus!
                    printdbg("IRQ %d on unknown bus ->"
                             " IOAPIC ID 0x%02x INTIN %u type=%d\n",
                           entry_iointr->source_bus_irq,
                           entry_iointr->dest_ioapic_id,
                           entry_iointr->dest_ioapic_intin,
                           entry_iointr->type);
                }

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
                    printdbg("PCI device %u INT_%c# ->"
                             " LAPIC ID 0x%02x INTIN %d\n",
                           device, (int)(pci_irq & 3) + 'A',
                           lapic_id, intin);
                } else if (entry_lintr->source_bus == mp_isa_bus_id) {
                    uint8_t isa_irq = entry_lintr->source_bus_irq;
                    uint8_t lapic_id = entry_lintr->dest_lapic_id;
                    uint8_t intin = entry_lintr->dest_lapic_lintin;

                    printdbg("ISA IRQ %d -> LAPIC ID 0x%02x INTIN %u\n",
                           isa_irq, lapic_id, intin);
                } else {
                    // Unknown bus!
                    printdbg("IRQ %d on unknown bus ->"
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

                printdbg("Address map, bus=%d, addr=%lx, len=%lx\n",
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

                printdbg("Bus hierarchy, bus=%d, parent=%d, info=%x\n",
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

                printdbg("Bus compat, bus=%d, mod=%d,"
                         " predefined_range_list=%x\n",
                         bus, bus_mod, bus_predef);

                entry += entry_buscompat->len;
                break;
            }

            default:
                printdbg("Unknown MP table entry_type!"
                         " Guessing size is 8\n");
                // Hope for the best here
                entry += 8;
                break;
            }
        }

        munmap(cth, 0x10000);
    }

    if (ranges[2] != 0)
        munmap(ranges[2], 0x20000);

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
    printdbg("Spurious APIC interrupt!\n");
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
            ? APIC_CMD_DEST_MODE_NORMAL
            : APIC_CMD_DEST_MODE_NMI;

    uint32_t dest = (target_apic_id >= 0) ? target_apic_id : 0;

    apic_send_command(dest, APIC_CMD_VECTOR_n(intr) | dest_type | dest_mode);
}

void apic_eoi(int intr)
{
    apic->write32(APIC_REG_EOI, intr & 0);
}

static void apic_online(int enabled, int spurious_intr)
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
}

void apic_dump_regs(int ap)
{
    for (int i = 0; i < 64; i += 4) {
        printdbg("ap=%d APIC: ", ap);
        for (int x = 0; x < 4; ++x) {
            if (apic->reg_readable(i + x)) {
                printdbg("[%3x]=%08x%s", (i + x),
                         apic->read32(i + x),
                         x == 3 ? "\n" : " ");
            } else {
                printdbg("[%3x]=--------%s", i + x,
                        x == 3 ? "\n" : " ");
            }
        }
    }
    printdbg("Logical destination register value: 0x%x\n",
             apic->read32(APIC_REG_LDR));
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
    uint64_t apic_base_msr = msr_get(APIC_BASE_MSR);

    if (!apic_base)
        apic_base = apic_base_msr & APIC_BASE_ADDR;

    if (!(apic_base_msr & APIC_BASE_GENABLE)) {
        printdbg("APIC was globally disabled!"
                 " Enabling...\n");
    }

    if (cpuid_has_x2apic()) {
        APIC_TRACE("Using x2APIC\n");
        if (!ap)
            apic = &apic_x2;

        msr_set(APIC_BASE_MSR, apic_base_msr |
                APIC_BASE_GENABLE | APIC_BASE_X2ENABLE);
    } else {
        APIC_TRACE("Using xAPIC\n");

        if (!ap) {
            // Bootstrap CPU only
            assert(!apic_ptr);
            apic_ptr = (uint32_t *)mmap((void*)(apic_base_msr & APIC_BASE_ADDR),
                                        4096, PROT_READ | PROT_WRITE,
                                        MAP_PHYSICAL | MAP_NOCACHE |
                                        MAP_WRITETHRU, -1, 0);

            apic = &apic_x;
        }

        msr_set(APIC_BASE_MSR, apic_base_msr | APIC_BASE_GENABLE);
    }

    // Set global enable if it is clear
    if (!(apic_base_msr & APIC_BASE_GENABLE)) {
        printdbg("APIC was globally disabled!"
                 " Enabling...\n");
    }

    apic_base_msr &= APIC_BASE_ADDR;

    if (!ap) {
        intr_hook(INTR_APIC_TIMER, apic_timer_handler);
        intr_hook(INTR_APIC_SPURIOUS, apic_spurious_handler);

        parse_mp_tables();

        apic_calibrate();
    }

    apic_online(1, INTR_APIC_SPURIOUS);

    apic->write32(APIC_REG_TPR, 0x0);

    assert(apic_base == (msr_get(APIC_BASE_MSR) & APIC_BASE_ADDR));

    if (ap) {
        apic_configure_timer(APIC_LVT_DCR_BY_1,
                             apic_timer_freq / 60,
                             APIC_LVT_TR_MODE_PERIODIC,
                             INTR_APIC_TIMER);
    }

    apic_dump_regs(ap);

    return 1;
}

static void apic_detect_topology(void)
{
    cpuid_t info;

    if (!cpuid(&info, 4, 0)) {
        // Enable full CPUID
        uint64_t misc_enables = msr_get(MSR_IA32_MISC_ENABLES);
        if (misc_enables & (1L<<22)) {
            // Enable more CPUID support and retry
            misc_enables &= ~(1L<<22);
            msr_set(MSR_IA32_MISC_ENABLES, misc_enables);
        }
    }

    topo_thread_bits = 0;
    topo_core_bits = 0;
    topo_thread_count = 1;
    topo_core_count = 1;

    if (cpuid(&info, 1, 0)) {
        if ((info.edx >> 28) & 1) {
            // CPU supports hyperthreading

            // Thread count
            topo_thread_count = (info.ebx >> 16) & 0xFF;
            while ((1U << topo_thread_bits) < topo_thread_count)
                 ++topo_thread_bits;
        }

        if (cpuid(&info, 4, 0)) {
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

    topo_cpu_count = apic_id_count *
            topo_core_count * topo_thread_count;
}

void apic_start_smp(void)
{
    // Start the timer here because interrupts are enable by now
    apic_configure_timer(APIC_LVT_DCR_BY_1,
                         apic_timer_freq / 60,
                         APIC_LVT_TR_MODE_PERIODIC,
                         INTR_APIC_TIMER);

    printdbg("%d CPU packages\n", apic_id_count);

    if (!acpi_rsdp_addr)
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
    uint32_t *mp_trampoline_ptr = (uint32_t*)0x7c40;
    uint32_t mp_trampoline_addr = *mp_trampoline_ptr;
    uint32_t mp_trampoline_page = mp_trampoline_addr >> 12;

    // Send INIT to all other CPUs
    apic_send_command(0xFFFFFFFF,
                      APIC_CMD_DEST_MODE_INIT |
                      APIC_CMD_DEST_LOGICAL |
                      APIC_CMD_DEST_TYPE_OTHER);

    sleep(10);

    printdbg("%d hyperthread bits\n", topo_thread_bits);
    printdbg("%d core bits\n", topo_core_bits);

    printdbg("%d hyperthread count\n", topo_thread_count);
    printdbg("%d core count\n", topo_core_count);

    uint32_t smp_expect = 0;
    for (unsigned pkg = 0; pkg < apic_id_count; ++pkg) {
        printdbg("Package base APIC ID = %u\n", apic_id_list[pkg]);

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

                printdbg("Sending IPI to APIC ID %u\n", target);

                // Send SIPI to CPU
                apic_send_command(target,
                                  APIC_CMD_SIPI_PAGE_n(
                                      mp_trampoline_page) |
                                  APIC_CMD_DEST_MODE_SIPI |
                                  APIC_CMD_DEST_TYPE_BYID);

                nsleep(stagger);

                ++smp_expect;
                while (thread_smp_running != smp_expect)
                    pause();
            }
        }
    }

    // SMP online
    callout_call('T');

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

    size_t get_size() const final { return size; }

    int64_t read() const final { return *mem; }

    void write(int64_t value) const final
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

    size_t get_size() const final { return size; }

    int64_t read() const final { return inp<size>(port); }

    void write(int64_t value) const final
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

    size_t get_size() const final { return size; }

    int64_t read() const final
    {
        value_type result;
        pci_config_copy(0, (dfo >> 32) & 0xFF,
                        (dfo >> 16) & 0xFF, &result,
                        dfo & 0xFF, size);
        return result;
    }

    void write(int64_t value) const final
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

static void apic_calibrate()
{
    int64_t wtf = acpi_pm_timer_raw();
    (void)wtf;
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

        // Wait for about 16ms
        do {
            pause();
            tmr_en = acpi_pm_timer_raw();
            tmr_diff = acpi_pm_timer_diff(tmr_st, tmr_en);
        } while (tmr_diff < 3579*16);

        uint32_t ccr_en = apic->read32(APIC_REG_LVT_CCR);
        uint64_t tsc_en = cpu_rdtsc();

        uint64_t tsc_elap = tsc_en - tsc_st;
        uint32_t ccr_elap = ccr_st - ccr_en;
        uint64_t tmr_nsec = acpi_pm_timer_ns(tmr_diff);

        uint64_t cpu_freq = (uint64_t(tsc_elap) * 1000000000) / tmr_nsec;
        uint64_t ccr_freq = (uint64_t(ccr_elap) * 1000000000) / tmr_nsec;

        apic_timer_freq = ccr_freq;

        // Round CPU frequency to nearest multiple of 33MHz
        cpu_freq += 8333333;
        cpu_freq -= cpu_freq % 16666666;

        // Round APIC frequency to nearest multiple of 100MHz
        apic_timer_freq += 50000000;
        apic_timer_freq -= apic_timer_freq % 100000000;

        rdtsc_mhz = (cpu_freq + 500000) / 1000000;

        // Example: let rdtsc_mhz = 2500. gcd(1000,2500) = 500
        // then,
        //  clk_to_ns_numer = 1000/500 = 2
        //  chk_to_ns_denom = 2500/500 = 5
        // clk_to_ns: let clks = 2500000000
        //  2500000000 * 2 / 5 = 1000000000ns

        uint64_t clk_to_ns_gcd = gcd(uint64_t(1000), rdtsc_mhz);
        clk_to_ns_numer = 1000 / clk_to_ns_gcd;
        clk_to_ns_denom = rdtsc_mhz / clk_to_ns_gcd;
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

    if (cpuid_has_inrdtsc()) {
        APIC_TRACE("Using RDTSC for precision timing\n");
        time_ns_set_handler(apic_rdtsc_time_ns_handler, nullptr, true);
    }

    printdbg("CPU clock: %luMHz\n", rdtsc_mhz);
    printdbg("APIC clock: %luHz\n", apic_timer_freq);
}

//
// IOAPIC

static void ioapic_lock_noirq(mp_ioapic_t *ioapic)
{
    spinlock_lock_noirq(&ioapic->lock);
}

static void ioapic_unlock_noirq(mp_ioapic_t *ioapic)
{
    spinlock_unlock_noirq(&ioapic->lock);
}

static uint32_t ioapic_read(mp_ioapic_t *ioapic, uint32_t reg)
{
    ioapic->ptr[IOAPIC_IOREGSEL] = reg;
    return ioapic->ptr[IOAPIC_IOREGWIN];
}

static void ioapic_write(mp_ioapic_t *ioapic,
                             uint32_t reg, uint32_t value)
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

// Returns 1 on success
// device should be 0 for ISA IRQs
static void ioapic_map(mp_ioapic_t *ioapic,
                       mp_bus_irq_mapping_t *mapping)
{
    uint8_t delivery;

    switch (mapping->intr_type) {
    case MP_INTR_TYPE_APIC:
        delivery = IOAPIC_REDLO_DELIVERY_APIC;
        break;

    case MP_INTR_TYPE_NMI:
        delivery = IOAPIC_REDLO_DELIVERY_NMI;
        break;

    case MP_INTR_TYPE_SMI:
        delivery = IOAPIC_REDLO_DELIVERY_SMI;
        break;

    case MP_INTR_TYPE_EXTINT:
        delivery = IOAPIC_REDLO_DELIVERY_EXTINT;
        break;

    default:
        printdbg("MP: Unrecognized interrupt delivery type!"
                 " Guessing APIC\n");
        delivery = IOAPIC_REDLO_DELIVERY_APIC;
        break;
    }

    uint8_t polarity;

    switch (mapping->flags & ACPI_MADT_ENT_IRQ_FLAGS_POLARITY) {
    default:
    case ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_n(
            ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVEHI):
        polarity = IOAPIC_REDLO_POLARITY_ACTIVEHI;
        break;
    case ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_n(
            ACPI_MADT_ENT_IRQ_FLAGS_POLARITY_ACTIVELO):
        polarity = IOAPIC_REDLO_POLARITY_ACTIVELO;
        break;
    }

    uint8_t trigger;

    switch (mapping->flags & ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER) {
    default:
        printdbg("MP: Unrecognized IRQ trigger type!"
                 " Guessing edge\n");
        // fall through...
    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(
            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_DEFAULT):
    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(
            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_EDGE):
        trigger = IOAPIC_REDLO_TRIGGER_EDGE;
        break;

    case ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_n(
            ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_LEVEL):
        trigger = IOAPIC_REDLO_TRIGGER_LEVEL;
        break;

    }

    uint8_t intr = ioapic->base_intr + mapping->intin;

    uint32_t iored_lo =
            IOAPIC_REDLO_VECTOR_n(intr) |
            IOAPIC_REDLO_DELIVERY_n(delivery) |
            IOAPIC_REDLO_POLARITY_n(polarity) |
            IOAPIC_REDLO_TRIGGER_n(trigger);

    IOAPIC_TRACE("Mapped IOAPIC irq %d (intin=%d) to interrupt 0x%x\n",
                 mapping->irq, mapping->intin, intr);

    uint32_t iored_hi = IOAPIC_REDHI_DEST_n(0);

    ioapic_lock_noirq(ioapic);

    // Write low part with mask set
    ioapic_write(ioapic, IOAPIC_RED_LO_n(mapping->intin),
                 iored_lo | IOAPIC_REDLO_MASKIRQ);

    atomic_barrier();

    // Write high part
    ioapic_write(ioapic, IOAPIC_RED_HI_n(mapping->intin), iored_hi);

    atomic_barrier();

    ioapic_unlock_noirq(ioapic);
}

//
//

static mp_ioapic_t *ioapic_from_intr(int intr)
{
    for (unsigned i = 0; i < ioapic_count; ++i) {
        mp_ioapic_t *ioapic = ioapic_list + i;
        if (intr >= ioapic->base_intr &&
                intr < ioapic->base_intr + ioapic->vector_count) {
            return ioapic;
        }
    }
    return 0;
}

static mp_bus_irq_mapping_t *ioapic_mapping_from_irq(int irq)
{
    return &bus_irq_list[bus_irq_to_mapping[irq]];
}

static isr_context_t *ioapic_dispatcher(int intr, isr_context_t *ctx)
{
    isr_context_t *orig_ctx = ctx;

    uint8_t irq;
    if (intr >= ioapic_msi_base_intr &&
            intr < INTR_APIC_SPURIOUS) {
        // MSI IRQ
        irq = intr - ioapic_msi_base_intr + ioapic_msi_base_irq;
    } else {
        mp_ioapic_t *ioapic = ioapic_from_intr(intr);
        assert(ioapic);
        if (!ioapic)
            return ctx;

        // IOAPIC IRQ

        mp_bus_irq_mapping_t *mapping = bus_irq_list.data();
        uint8_t intin = intr - ioapic->base_intr;
        for (irq = 0; irq < bus_irq_list.size(); ++irq, ++mapping) {
            if (mapping->ioapic_id == ioapic->id &&
                    mapping->intin == intin)
                break;
        }

        // Reverse map ISA IRQ
        uint8_t *isa_match = (uint8_t *)memchr(isa_irq_lookup, irq,
                                               sizeof(isa_irq_lookup));

        if (isa_match)
            irq = isa_match - isa_irq_lookup;
        else
            irq = ioapic->irq_base + intin;
    }

    apic_eoi(intr);
    ctx = (isr_context_t*)irq_invoke(intr, irq, ctx);

    if (ctx == orig_ctx)
        return thread_schedule_if_idle(ctx);

    return ctx;
}

static void ioapic_setmask(int irq, bool unmask)
{
    mp_bus_irq_mapping_t *mapping = ioapic_mapping_from_irq(irq);
    mp_ioapic_t *ioapic = ioapic_by_id(mapping->ioapic_id);

    ioapic_lock_noirq(ioapic);

    uint32_t ent = ioapic_read(ioapic, IOAPIC_RED_LO_n(mapping->intin));

    if (unmask)
        ent &= ~IOAPIC_REDLO_MASKIRQ;
    else
        ent |= IOAPIC_REDLO_MASKIRQ;

    ioapic_write(ioapic, IOAPIC_RED_LO_n(mapping->intin), ent);

    ioapic_unlock_noirq(ioapic);
}

static void ioapic_hook(int irq, intr_handler_t handler)
{
    uint8_t intr;
    if (irq >= ioapic_msi_base_irq) {
        // MSI IRQ
        intr = irq - ioapic_msi_base_irq + ioapic_msi_base_intr;
    } else {
        // IOAPIC IRQ
        mp_bus_irq_mapping_t *mapping = ioapic_mapping_from_irq(irq);
        mp_ioapic_t *ioapic = ioapic_by_id(mapping->ioapic_id);
        intr = ioapic->base_intr + mapping->intin;
    }
    intr_hook(intr, handler);
}

static void ioapic_unhook(int irq, intr_handler_t handler)
{
    mp_bus_irq_mapping_t *mapping = ioapic_mapping_from_irq(irq);
    mp_ioapic_t *ioapic = ioapic_by_id(mapping->ioapic_id);
    uint8_t intr = ioapic->base_intr + mapping->intin;
    intr_unhook(intr, handler);
}

static void ioapic_map_all(void)
{
    mp_bus_irq_mapping_t *mapping;
    mp_ioapic_t *ioapic;

    for (unsigned i = 0; i < 16; ++i)
        bus_irq_to_mapping[i] = isa_irq_lookup[i];

    for (size_t i = 0; i < bus_irq_list.size(); ++i) {
        mapping = &bus_irq_list[i];
        ioapic = ioapic_by_id(mapping->ioapic_id);
        if (mapping->bus != mp_isa_bus_id) {
            uint8_t irq = ioapic->irq_base + mapping->intin;
            bus_irq_to_mapping[irq] = i;
        }

        ioapic_map(ioapic, mapping);
    }

    // MSI IRQs start after last IOAPIC
    ioapic = &ioapic_list[ioapic_count-1];
    ioapic_msi_base_intr = ioapic->base_intr +
            ioapic->vector_count;
    ioapic_msi_base_irq = ioapic->irq_base +
            ioapic->vector_count;
}

// Pass negative cpu value to get highest CPU number
// Returns 0 on failure, 1 on success
bool ioapic_irq_cpu(int irq, int cpu)
{
    if (cpu < 0)
        return ioapic_count > 0;

    if (unsigned(cpu) >= apic_id_count)
        return false;

    mp_bus_irq_mapping_t *mapping = ioapic_mapping_from_irq(irq);
    mp_ioapic_t *ioapic = ioapic_by_id(mapping->ioapic_id);
    ioapic_lock_noirq(ioapic);
    ioapic_write(ioapic, IOAPIC_RED_HI_n(mapping->intin),
                 IOAPIC_REDHI_DEST_n(apic_id_list[cpu]));
    ioapic_unlock_noirq(ioapic);
    return true;
}

int apic_enable(void)
{
    if (ioapic_count == 0)
        return 0;

    ioapic_map_all();

    irq_dispatcher_set_handler(ioapic_dispatcher);
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
                       int cpu, int distribute,
                       intr_handler_t handler)
{
    // Don't try to use MSI if there are no IOAPIC devices
    if (ioapic_count == 0)
        return 0;

    // If out of range starting CPU number, force to zero
    if (cpu < 0 || (unsigned)cpu >= apic_id_count)
        cpu = 0;

    // Only allow distribute to be 0 or 1, fail otherwise
    if (distribute < 0 || distribute > 1)
        return 0;

    uint8_t vector_base = ioapic_aligned_vectors(bit_log2_n(count));

    // See if we ran out of vectors
    if (vector_base == 0)
        return 0;

    for (int i = 0; i < count; ++i) {
        results[i].addr = (0xFEEU << 20) |
                (apic_id_list[cpu] << 12);
        results[i].data = (vector_base + i);

        irq_hook(vector_base + i - ioapic_msi_base_intr +
                 ioapic_msi_base_irq,
                 handler);

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
