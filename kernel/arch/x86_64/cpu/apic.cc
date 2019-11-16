#include "apic.h"
#include "debug.h"
#include "numeric.h"
#include "cpu/asm_constants.h"
#include "cpu/mptables.h"
#include "cpu/ioapic.h"
#include "device/acpigas.h"
#include "gdt.h"
#include "bios_data.h"
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
#include "work_queue.h"
#include "stdlib.h"

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
#define APIC_TRACE(...) printdbg("lapic: " __VA_ARGS__)
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

static char const * const intr_type_text[] = {
    "APIC",
    "NMI",
    "SMI",
    "EXTINT"
};

struct mp_ioapic_t {
    uint8_t id;
    uint8_t base_intr;
    uint8_t vector_count;
    uint8_t irq_base;
    uint32_t addr;
    uint32_t volatile *ptr;
    using lock_type = ext::mcslock;
    using scoped_lock = std::unique_lock<ext::mcslock>;
    lock_type lock;
};

static char const *mp_tables;

static std::vector<uint8_t> mp_pci_bus_ids;
static uint16_t mp_isa_bus_id;

static uint64_t apic_timer_freq;

static unsigned ioapic_count;
static mp_ioapic_t ioapic_list[16];

using ioapic_msi_alloc_lock_type = ext::mcslock;
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
static bool ioapic_set_flags(mp_ioapic_t *ioapic,
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
static int16_t irq_is_level[256];
static int16_t irq_manual_eoi[256];

static uint8_t topo_thread_bits;
static uint8_t topo_thread_count;
static uint8_t topo_core_bits;
static uint8_t topo_core_count;

static uint8_t topo_cpu_count;

static uintptr_t apic_base;
static uint32_t volatile *apic_ptr;

struct acpi_mapping_t {
    uint64_t base;
    uint64_t len;

    acpi_mapping_t()
        : base{}
        , len{}
    {
    }

    acpi_mapping_t(uint64_t base, uint64_t len)
        : base{base}
        , len{len}
    {
    }

    bool operator==(acpi_mapping_t const& rhs)
    {
        return (base == rhs.base) & (len == rhs.len);
    }

    bool operator!=(acpi_mapping_t const& rhs)
    {
        return (base != rhs.base) | (len != rhs.len);
    }
};

static std::vector<acpi_mapping_t> acpi_mappings;

struct memory_affinity_t {
    uint64_t base;
    uint64_t length;
    uint32_t domain;
};

static std::vector<memory_affinity_t> acpi_mem_affinity;

struct apic_affinity_t {
    uint32_t domain;
    uint32_t apic_id;
};

static std::vector<apic_affinity_t> acpi_apic_affinity;

static uint64_t acpi_slit_localities;
static std::vector<uint8_t> acpi_slit_table;

// SRAT

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
    virtual void command(uint32_t dest, uint32_t cmd) = 0;

    virtual uint32_t read32(uint32_t offset) = 0;
    virtual void write32(uint32_t offset, uint32_t val) = 0;

    virtual uint64_t read64(uint32_t offset) = 0;
    virtual void write64(uint32_t offset, uint64_t val) = 0;

    virtual void command_noinst(uint32_t dest, uint32_t cmd) = 0;

    virtual uint32_t read32_noinst(uint32_t offset) = 0;
    virtual void write32_noinst(uint32_t offset, uint32_t val) = 0;

    virtual uint64_t read64_noinst(uint32_t offset) = 0;
    virtual void write64_noinst(uint32_t offset, uint64_t val) = 0;

    virtual bool reg_readable(uint32_t reg) = 0;
    virtual bool reg_exists(uint32_t reg) = 0;

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
    void command(uint32_t dest, uint32_t cmd) override final
    {
        cpu_scoped_irq_disable intr_enabled;
        write32(APIC_REG_ICR_HI, APIC_DEST_n(dest));
        write32(APIC_REG_ICR_LO, cmd);
        intr_enabled.restore();
        while (read32(APIC_REG_ICR_LO) & APIC_CMD_PENDING)
            pause();
    }

    _no_instrument _flatten
    void command_noinst(uint32_t dest, uint32_t cmd) override final
    {
        bool irq_en = cpu_irq_save_disable_noinst();
        write32_noinst(APIC_REG_ICR_HI, APIC_DEST_n(dest));
        write32_noinst(APIC_REG_ICR_LO, cmd);
        cpu_irq_toggle_noinst(irq_en);
        while (read32_noinst(APIC_REG_ICR_LO) & APIC_CMD_PENDING)
            __builtin_ia32_pause();
    }

    uint32_t read32(uint32_t offset) override final
    {
        return apic_ptr[offset << (4 - 2)];
    }

    _no_instrument
    uint32_t read32_noinst(uint32_t offset) override final
    {
        return apic_ptr[offset << (4 - 2)];
    }

    void write32(uint32_t offset, uint32_t val) override final
    {
        apic_ptr[offset << (4 - 2)] = val;
    }

    _no_instrument
    void write32_noinst(uint32_t offset, uint32_t val) override final
    {
        apic_ptr[offset << (4 - 2)] = val;
    }

    uint64_t read64(uint32_t offset) override final
    {
        return ((uint64_t*)apic_ptr)[offset << (4 - 3)];
    }

    _no_instrument
    uint64_t read64_noinst(uint32_t offset) override final
    {
        return ((uint64_t*)apic_ptr)[offset << (4 - 3)];
    }

    void write64(uint32_t offset, uint64_t val) override final
    {
        ((uint64_t*)apic_ptr)[offset << (4 - 3)] = val;
    }

    _no_instrument
    void write64_noinst(uint32_t offset, uint64_t val) override final
    {
        ((uint64_t*)apic_ptr)[offset << (4 - 3)] = val;
    }

    bool reg_readable(uint32_t reg) override final
    {
        return reg_maybe_readable(reg);
    }

    bool reg_exists(uint32_t reg) override final
    {
        return reg_maybe_readable(reg);
    }
};

class lapic_x2_t : public lapic_t {
protected:
    void command(uint32_t dest, uint32_t cmd) override final
    {
        write64(APIC_REG_ICR_LO, (uint64_t(dest) << 32) | cmd);
    }

    _no_instrument
    void command_noinst(uint32_t dest, uint32_t cmd) override final
    {
        write64_noinst(APIC_REG_ICR_LO, (uint64_t(dest) << 32) | cmd);
    }

    uint32_t read32(uint32_t offset) override final
    {
        return cpu_msr_get_lo(0x800 + offset);
    }

    _no_instrument
    uint32_t read32_noinst(uint32_t offset) override final
    {
        return cpu_msr_get_lo(0x800 + offset);
    }

    void write32(uint32_t offset, uint32_t val) override
    {
        cpu_msr_set(0x800 + offset, val);
    }

    _no_instrument
    void write32_noinst(uint32_t offset, uint32_t val) override final
    {
        cpu_msr_set(0x800 + offset, val);
    }

    uint64_t read64(uint32_t offset) override final
    {
        return cpu_msr_get(0x800 + offset);
    }

    _no_instrument
    uint64_t read64_noinst(uint32_t offset) override final
    {
        return cpu_msr_get(0x800 + offset);
    }

    void write64(uint32_t offset, uint64_t val) override final
    {
        cpu_msr_set(0x800 + offset, val);
    }

    _no_instrument
    void write64_noinst(uint32_t offset, uint64_t val) override final
    {
        cpu_msr_set(0x800 + offset, val);
    }

    bool reg_readable(uint32_t reg) override final
    {
        // APIC_REG_LVT_CMCI raises #GP if CMCI not enabled
        return reg != APIC_REG_LVT_CMCI &&
                reg != APIC_REG_ICR_HI &&
                reg_exists(reg) &&
                reg_maybe_readable(reg);
    }

    bool reg_exists(uint32_t reg) override final
    {
        return reg != APIC_REG_DFR &&
                reg != APIC_REG_APR &&
                reg_maybe_exists(reg);
    }
};

class lapic_kvm_t : public lapic_x2_t {
public:
    lapic_kvm_t();
    ~lapic_kvm_t();

    void write32(uint32_t offset, uint32_t val) override final;

private:
    // Each CPUs paravirtualized EOI address is on its own cache
    // line. Only bit zero of value[0] is actually used.
    // When KVM issues a paravirtualized IRQ, it sets bit 0
    // of values[0] to 1. The kernel should test that bit to
    // see if KVM is issuing a paravirtualized IRQ. If that
    // bit is 1, then the kernel should exchange it with zero.
    // The test and reset should be atomic.

    struct cacheline_t {
        uint32_t values[CPUM_CACHELINESIZE / sizeof(uint32_t)];
    };

    static constexpr uint32_t const msr_kvm_eoi = 0x4b564d04;
    void paravirt_eoi();

    std::unique_ptr<cacheline_t[]> cpus;
    size_t cpu_count;
};

union lapic_both_t {
    lapic_x_t apic_x;
    lapic_x2_t apic_x2;
    lapic_kvm_t apic_kvm;

    struct xapic_tag_t {};
    struct x2apic_tag_t {};
    struct kvmpvapic_tag_t {};

    lapic_both_t()
    {
    }

    lapic_both_t(xapic_tag_t)
        : apic_x{}
    {
    }

    lapic_both_t(x2apic_tag_t)
        : apic_x2{}
    {
    }

    lapic_both_t(kvmpvapic_tag_t)
        : apic_kvm{}
    {
    }
};

static lapic_x_t apic_x;
static lapic_x2_t apic_x2;
static lapic_t *apic;

//.....


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
            ACPI_TRACE("Got ACPI_MADT_REC_TYPE_LAPIC\n");
            break;

        case ACPI_MADT_REC_TYPE_IOAPIC:
            ACPI_TRACE("Got ACPI_MADT_REC_TYPE_IOAPIC\n");
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
            irq_is_level[irq] = ioapic_set_flags(
                        ioapic, irq - ioapic->irq_base,
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


// Sometimes we have to guess the size
// then read the header to get the actual size.
// This handles that.
template<typename T>
static T *acpi_remap_len(T *ptr, uintptr_t physaddr,
                         size_t guess, size_t actual_len)
{
    if (actual_len > guess) {
        auto it = std::find(acpi_mappings.begin(), acpi_mappings.end(),
                            acpi_mapping_t{uint64_t(ptr), uint64_t(guess)});
        assert(it != acpi_mappings.end());
        acpi_mappings.erase(it);
        munmap(ptr, guess);
        ptr = (T*)mmap((void*)physaddr, actual_len,
                       PROT_READ, MAP_PHYSICAL, -1, 0);
        acpi_mappings.push_back({uint64_t(ptr), uint64_t(actual_len)});
    }

    return ptr;
}

static void acpi_parse_rsdt()
{
    // Sanity check length (<= 1MB)
    assert(acpi_rsdt_len <= (1 << 20));

    size_t len = acpi_rsdt_len ? acpi_rsdt_len : sizeof(acpi_sdt_hdr_t);
    acpi_sdt_hdr_t *rsdt_hdr = (acpi_sdt_hdr_t *)mmap(
                (void*)acpi_rsdt_addr, len,
                PROT_READ, MAP_PHYSICAL, -1, 0);
    acpi_mappings.push_back({uint64_t(rsdt_hdr), len});

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

        if (acpi_rsdt_ptrsz == sizeof(uint32_t)){
            hdr_addr = *rsdp_ptr;
        } else {
            memcpy(&hdr_addr, rsdp_ptr, sizeof(uint64_t));
        }

        acpi_sdt_hdr_t *hdr = (acpi_sdt_hdr_t *)
                mmap((void*)hdr_addr,
                      64 << 10, PROT_READ, MAP_PHYSICAL, -1, 0);
        acpi_mappings.push_back({uint64_t(hdr), uint64_t{64 << 10}});

        hdr = acpi_remap_len(hdr, hdr_addr, 64 << 10, hdr->len);

        ACPI_TRACE("Encountered sig=%4.4s\n", hdr->sig);

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
        } else if (!memcmp(hdr->sig, "MCFG", 4) /* && false killed */) {
            acpi_mcfg_hdr_t *mcfg_hdr = (acpi_mcfg_hdr_t*)hdr;

            if (acpi_chk_hdr(&mcfg_hdr->hdr) == 0) {
                acpi_ecam_rec_t *ecam_ptr = (acpi_ecam_rec_t*)
                        (mcfg_hdr+1);
                acpi_ecam_rec_t *ecam_end = (acpi_ecam_rec_t*)
                        ((char*)&mcfg_hdr->hdr + mcfg_hdr->hdr.len);
                size_t ecam_count = ecam_end - ecam_ptr;
                pci_init_ecam(ecam_count);
                for (size_t i = 0; i < ecam_count; ++i) {
                    acpi_ecam_rec_t item;
                    memcpy(&item, ecam_ptr, sizeof(item));

                    ACPI_TRACE("PCIe ECAM ptr=%#" PRIx64 " busses=%u-%u\n",
                               item.ecam_base,
                               item.st_bus, item.en_bus);
                    pci_init_ecam_entry(item.ecam_base,
                                        item.segment_group,
                                        item.st_bus,
                                        item.en_bus);
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
                                   ", clk_domain=%u"
                                   "\n",
                                   lapic_rec->domain_lo |
                                   (lapic_rec->domain_hi[0] << 8) |
                                   (lapic_rec->domain_hi[1] << 16) |
                                   (lapic_rec->domain_hi[2] << 24),
                                   lapic_rec->apic_id,
                                   lapic_rec->flags,
                                   lapic_rec->clk_domain);
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

                        if (unlikely(!acpi_mem_affinity.
                                     push_back({mem_rec->range_base,
                                               mem_rec->range_length,
                                               mem_rec->domain})))
                            panic_oom();

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

                        if (unlikely(!acpi_apic_affinity.
                                     push_back({x2apic_rec->domain,
                                               x2apic_rec->x2apic_id})))
                            panic_oom();

                        break;

                    default:
                        ACPI_TRACE("Got unrecognized affinity record\n");
                        break;

                    }
                }
            }
        } else if (!memcmp(hdr->sig, "SLIT", 4)) {
            acpi_slit_t *slit = (acpi_slit_t *)hdr;

            acpi_slit_localities = slit->locality_count;
            if (unlikely(!acpi_slit_table.assign(
                             (char*)(slit + 1),
                             (char*)(slit + 1) +
                             (acpi_slit_localities *
                              acpi_slit_localities))))
                panic_oom();
        } else {
            if (acpi_chk_hdr(hdr) == 0) {
                ACPI_TRACE("ACPI %4.4s ignored\n", hdr->sig);
            } else {
                ACPI_ERROR("ACPI %4.4s checksum mismatch!"
                       " (would have ignored anyway)\n", hdr->sig);
            }
        }

        //munmap(hdr, std::max(size_t(64) << 10, size_t(hdr->len)));
    }
}

static void mp_parse_fps()
{
    mp_cfg_tbl_hdr_t *cth = (mp_cfg_tbl_hdr_t *)
            mmap((void*)mp_tables, 0x10000,
                 PROT_READ, MAP_PHYSICAL, -1, 0);
    acpi_mappings.push_back({uint64_t(cth), 0x10000});

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

    //munmap(cth, std::max(0x10000, cth->base_tbl_len + cth->ext_tbl_len));
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

_hot
static isr_context_t *apic_timer_handler(int intr, isr_context_t *ctx)
{
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

size_t apic_cpu_count()
{
    return apic_id_count;
}

static void apic_send_command(uint32_t dest, uint32_t cmd)
{
    apic->command(dest, cmd);
}

_no_instrument
static void apic_send_command_noinst(uint32_t dest, uint32_t cmd)
{
    apic->command_noinst(dest, cmd);
}

_hot
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

_hot _no_instrument
void apic_send_ipi_noinst(int target_apic_id, uint8_t intr)
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

    apic_send_command_noinst(
                dest, APIC_CMD_VECTOR_n(intr) | dest_type | dest_mode);
}

_hot
void apic_eoi(int intr)
{
    apic->write32(APIC_REG_EOI, intr & 0);
}

_no_instrument
void apic_eoi_noinst(int intr)
{
    apic->write32_noinst(APIC_REG_EOI, intr & 0);
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

static const constexpr uint8_t apic_shr_to_dcr[] = {
    APIC_LVT_DCR_BY_1,
    APIC_LVT_DCR_BY_2,
    APIC_LVT_DCR_BY_4,
    APIC_LVT_DCR_BY_8,
    APIC_LVT_DCR_BY_16,
    APIC_LVT_DCR_BY_32,
    APIC_LVT_DCR_BY_64,
    APIC_LVT_DCR_BY_128
};

// The maximum possible ticks value is 32+7=39 bits
// 0x7FFFFFFF80
// Rounds to nearest when resolution is insufficient due to divisor
// Returns actual wait (possibly rounded off) wait time
// in APIC timer ticks
EXPORT uint64_t apic_configure_timer(uint64_t ticks, bool one_shot, bool mask)
{
    if (ticks <= INT32_MAX) {
        apic_configure_timer(APIC_LVT_DCR_BY_1, ticks,
                             one_shot ? APIC_LVT_TR_MODE_ONESHOT
                                      : APIC_LVT_TR_MODE_PERIODIC,
                             INTR_APIC_TIMER, mask);
        return ticks;
    }

    static constexpr const uint64_t ticks_max =
            (UINT64_C(0xFFFFFFFF) << (countof(apic_shr_to_dcr) - 1));

    // If counter is not representable, cap to maximum
    ticks = ticks < ticks_max
            ? ticks
            : ticks_max;

    // Determine the highest and lowest set bits
    uint8_t lsb_set = bit_lsb_set(ticks);
    uint8_t msb_set = bit_msb_set(ticks);

    // Must shift at least this much to fit in 32 bit counter
    // This is the minimum shift which maintains range
    uint8_t min_shr = msb_set > 31 ? msb_set - 31 : 0;

    // Would be nice to shift this much to keep full precision
    // This is the maximum shift which maintains precision
    uint8_t max_shr = lsb_set;

    // min_shr takes precedence over max_shr to shift more
    uint8_t shift = max_shr < min_shr ? min_shr : max_shr;

    bool round_bit = shift && (ticks & (UINT64_C(1) << (shift - 1)));
    ticks >>= shift;
    ticks += round_bit;

    // Rounding might carry to next shift
    round_bit = unlikely(ticks > UINT64_C(0xFFFFFFFF));
    ticks >>= uint8_t(round_bit);
    shift += uint8_t(round_bit);

    if (unlikely(shift > 7)) {
        ticks = 0xFFFFFFFF;
        shift = 7;
    }

    uint32_t dcr = apic_shr_to_dcr[shift];

    apic_configure_timer(dcr, ticks,
                         one_shot ? APIC_LVT_TR_MODE_ONESHOT
                                  : APIC_LVT_TR_MODE_PERIODIC,
                         INTR_APIC_TIMER, mask);

    return ticks << shift;
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
        intr_hook(INTR_APIC_TIMER, apic_timer_handler,
                  "apic_timer", eoi_lapic);
        intr_hook(INTR_APIC_SPURIOUS, apic_spurious_handler,
                  "apic_spurious", eoi_lapic);
        intr_hook(INTR_APIC_ERROR, apic_err_handler,
                  "apic_error", eoi_lapic);

        APIC_TRACE("Parsing boot tables\n");

        parse_mp_tables();

        APIC_TRACE("Calibrating APIC timer (twice)\n");

        // Twice, once for warmup, once to actually calibrate
        apic_calibrate();
        apic_calibrate();
    }

    APIC_TRACE("Enabling APIC\n");

    apic_online(1, INTR_APIC_SPURIOUS, INTR_APIC_ERROR);

    // Initialize task priority register to zero, unmasking all IRQs
    apic->write32(APIC_REG_TPR, 0x0);

    assert(apic_base == (cpu_msr_get(CPU_APIC_BASE_MSR) & CPU_APIC_BASE_ADDR));

    if (ap) {
        APIC_TRACE("Configuring AP timer\n");
        apic_configure_timer(APIC_LVT_DCR_BY_1,
                             apic_timer_freq / 20,
                             APIC_LVT_TR_MODE_PERIODIC,
                             INTR_APIC_TIMER);
    }

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

    thread_init_cpu_count(apic_id_count);

    // Populate critical cpu local info on every cpu that is needed very early
    for (size_t i = 0; i < apic_id_count; ++i)
        thread_init_cpu(i, apic_id_list[i]);

    // See if there are any other CPUs to start
    if (topo_thread_count * topo_core_count == 1 &&
            apic_id_count == 1)
        return;

    // Read address of MP entry trampoline from boot sector
    uint32_t mp_trampoline_addr = (uint32_t)
            bootinfo_parameter(bootparam_t::ap_entry_point);
    uint32_t mp_trampoline_page = mp_trampoline_addr >> 12;

    cmos_prepare_ap();

    // Send INIT to all other CPUs
    apic_send_command(0, APIC_CMD_DELIVERY_INIT |
                      APIC_CMD_DEST_MODE_LOGICAL |
                      APIC_CMD_DEST_TYPE_OTHER);
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

                APIC_TRACE("BSP waiting for AP\n");
                cpu_wait_value(&thread_smp_running, smp_expect);
                APIC_TRACE("BSP finished waiting for AP\n");
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
    if (0 && cpuid_is_hypervisor() && cpuid(&info, 0x40000000, 0) &&
            info.eax >= 0x40000010 && cpuid(&info, 0x40000010, 0)) {
        rdtsc_freq = (info.eax + 500) * 1000;
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

        // Wait for almost 24ms
        unsigned iter = 0;
        do {
            pause();
            tmr_en = acpi_pm_timer_raw();
            tmr_diff = acpi_pm_timer_diff(tmr_st, tmr_en);
            ++iter;
        } while (tmr_diff < (ACPI_PM_TIMER_HZ/42));

        uint32_t ccr_en = apic->read32(APIC_REG_LVT_CCR);
        uint64_t tsc_en = cpu_rdtsc();

        uint64_t tsc_elap = tsc_en - tsc_st;
        uint64_t ccr_elap = ccr_st - ccr_en;
        uint64_t tmr_nsec = acpi_pm_timer_ns(tmr_diff);

        uint64_t cpu_freq = (tsc_elap * 1000000000) / tmr_nsec;
        uint64_t ccr_freq = (ccr_elap * 1000000000) / tmr_nsec;

        apic_timer_freq = ccr_freq;

        // Round APIC frequency to nearest multiple of 333kHz
//        apic_timer_freq += 166666;
//        apic_timer_freq -= apic_timer_freq % 333333;

        // APIC frequency < 333kHz is impossible to believe
        assert(apic_timer_freq > 333333);

        // Round CPU frequency to nearest multiple of 1MHz
        cpu_freq += 500000;
        rdtsc_freq = cpu_freq;
    } else {
        // Program timer (should be high enough to measure 858ms @ 5GHz)
        apic_configure_timer(APIC_LVT_DCR_BY_1, 0xFFFFFFF0U,
                             APIC_LVT_TR_MODE_n(APIC_LVT_TR_MODE_ONESHOT),
                             INTR_APIC_TIMER, true);

        // Note starting values of both the timer and the timestamp counter
        uint32_t ccr_st = apic->read32(APIC_REG_LVT_CCR);
        uint64_t tsc_st = cpu_rdtsc();

        // Wait for about 10ms
        uint64_t tmr_nsec = nsleep(10000000);

        // Note the ending values of the timer and the timestamp counter
        uint32_t ccr_en = apic->read32(APIC_REG_LVT_CCR);
        uint64_t tsc_en = cpu_rdtsc();

        // Compute how many ticks occurred on both timers
        uint64_t tsc_elap = tsc_en - tsc_st;
        uint64_t ccr_elap = ccr_st - ccr_en;

        // Extrapolate how many ticks per second will occur on both timers
        uint64_t cpu_freq = (tsc_elap * 1000000000) / tmr_nsec;
        uint64_t ccr_freq = (ccr_elap * 1000000000) / tmr_nsec;

        // Cheapest crystals found online were +/- 50ppm stability, and
        // +/- 30ppm tolerance, so they may be out as much as 80ppm
        // low=0.99992*1193181.6...=1193086
        //  hi=1.00008*1193181.6...=1193277
        // varies 191Hz
        //

        // Remember both frequencies
        apic_timer_freq = ccr_freq;
        rdtsc_freq = cpu_freq;
    }

    // Example: let rdtsc_mhz = 2500. gcd(1000,2500) = 500
    // then,
    //  clk_to_ns_numer = 1000/500 = 2
    //  chk_to_ns_denom = 2500/500 = 5
    // clk_to_ns: let clks = 2500000000
    //  2500000000 * 2 / 5 = 1000000000ns

    uint64_t clk_to_ns_gcd = std::gcd(uint64_t(1000000000), rdtsc_freq);

    //APIC_TRACE("CPU MHz GCD: %" PRId64 "\n", clk_to_ns_gcd);

    clk_to_ns_numer = 1000000000 / clk_to_ns_gcd;
    clk_to_ns_denom = rdtsc_freq / clk_to_ns_gcd;

    //APIC_TRACE("clk_to_ns_numer: %" PRId64 "\n", clk_to_ns_numer);
    //APIC_TRACE("clk_to_ns_denom: %" PRId64 "\n", clk_to_ns_denom);

    if (cpuid_is_hypervisor() || cpuid_has_inrdtsc()) {
        APIC_TRACE("Using RDTSC for precision timing\n");
        if (!cpuid_has_inrdtsc())
            APIC_TRACE("Using RDTSC overriding cpuid because hyperivisor\n");
        time_ns_set_handler(apic_rdtsc_time_ns_handler, nullptr, true);
        nsleep_set_handler(apic_rdtsc_nsleep_handler, nullptr, true);
    }

    APIC_TRACE("CPU clock: %" PRIu64 "Hz (%" PRIu64 ".%02u GHz)\n", rdtsc_freq,
               rdtsc_freq / 1000000000,
               unsigned((rdtsc_freq / 10000000) % 100));
    APIC_TRACE("APIC clock: %" PRIu64 "Hz (%" PRIu64 ".%02u MHz)\n",
               apic_timer_freq, apic_timer_freq / 1000000,
               unsigned((apic_timer_freq / 10000) % 100));
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
    cpu_scoped_irq_disable irq_dis;
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
    // Later overrides may remap these
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
        ioapic_write(ioapic, IOAPIC_RED_HI_n(i),
                     IOAPIC_REDHI_DEST_n(apic_id_list[0]), lock);
    }
}

static void ioapic_set_type(mp_ioapic_t *ioapic,
                            uint8_t intin, uint8_t intr_type)
{
    cpu_scoped_irq_disable irq_dis;
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

// Returns true if the irq is level triggered
static bool ioapic_set_flags(mp_ioapic_t *ioapic,
                             uint8_t intin, uint16_t intr_flags, bool isa)
{
    cpu_scoped_irq_disable irq_dis;
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

    return trigger & ACPI_MADT_ENT_IRQ_FLAGS_TRIGGER_LEVEL;
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

_hot
isr_context_t *apic_dispatcher(int intr, isr_context_t *ctx)
{
    //APIC_TRACE("Dispatching IRQ %d\n", intr);
    //intr_handler_names(intr);

    uint64_t st = cpu_rdtsc();

    assert(intr >= INTR_APIC_DSP_BASE);
    assert(intr < INTR_APIC_IRQ_END);

    isr_context_t *orig_ctx = ctx;

    int irq = intr < INTR_APIC_IRQ_BASE ? intr : intr_to_irq[intr];

    assert(intr < INTR_APIC_IRQ_BASE || irq >= 0);
    assert(irq < INTR_APIC_IRQ_COUNT);

    ctx = irq_invoke(intr, irq, ctx);

    if (!irq_manual_eoi[irq])
        apic_eoi(intr);

    uint64_t en = cpu_rdtsc();
    en -= st;

    thread_add_cpu_irq_time(en);

    if (likely(ctx == orig_ctx))
        ctx = thread_schedule_postirq(ctx);

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

    cpu_scoped_irq_disable irq_dis;
    mp_ioapic_t::scoped_lock lock(ioapic->lock);

    uint32_t ent = ioapic_read(ioapic, IOAPIC_RED_LO_n(intin), lock);

    if (unmask)
        ent &= ~IOAPIC_REDLO_MASKIRQ;
    else
        ent |= IOAPIC_REDLO_MASKIRQ;

    ioapic_write(ioapic, IOAPIC_RED_LO_n(intin), ent, lock);
}

static bool ioapic_islevel(int irq)
{
    assert(irq >= 0);
    assert(irq < 256);
    return irq_is_level[irq];
}

static void ioapic_hook(int irq, intr_handler_t handler,
                        char const *name)
{
    int intr = irq_to_intr[irq];
    assert(intr >= 0);
    if (intr >= 0) {
        intr_hook(intr, handler, name, eoi_lapic);
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

void ioapic_irq_unhandled(int irq, int intr)
{
    apic_eoi(intr);
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
    irq_islevel_set_handler(ioapic_islevel);
    irq_hook_set_handler(ioapic_hook);
    irq_unhook_set_handler(ioapic_unhook);
    irq_setcpu_set_handler(ioapic_irq_setcpu);
    irq_set_unhandled_irq_handler(ioapic_irq_unhandled);

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
    }

    return vector_base - INTR_APIC_IRQ_BASE;
}

uint32_t acpi_cpu_count()
{
    return apic_id_count;
}

lapic_kvm_t::lapic_kvm_t()
{
    cpu_count = thread_cpu_count();
    cpus.reset(new (std::nothrow) cacheline_t[cpu_count]);

    // Initialize the MSR on every CPU
    workq::enqueue_on_all_barrier([&] (size_t i) {
        // [63:2] = physical address of 32 bit paravirt dword
        // [1] = reserved, MBZ
        // [0] = enable paravirtualized EOI
        uint64_t kvm_eoi = mphysaddr(&cpus[i].values[0]);
        kvm_eoi |= 1;
        cpu_msr_set(msr_kvm_eoi, kvm_eoi);
    });
}

lapic_kvm_t::~lapic_kvm_t()
{

}

void lapic_kvm_t::write32(uint32_t offset, uint32_t val)
{
    // Redirect EOI write to paravirtualized EOI
    if (offset == APIC_REG_EOI)
        paravirt_eoi();

    return lapic_x2_t::write32(offset, val);
}

void lapic_kvm_t::paravirt_eoi()
{
    size_t cpu = thread_cpu_number();
    // It was not set, cannot do paravirtualized EOI
    if (unlikely(!atomic_btr(&cpus[cpu].values[0], 0)))
        lapic_x2_t::write32(APIC_REG_EOI, 0);
}
