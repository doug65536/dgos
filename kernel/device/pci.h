#pragma once
#include "types.h"
#include "pci.bits.h"
#include "assert.h"
#include "irq.h"
#include "string.h"
#include "vector.h"

struct pci_addr_t {
    // Legacy PCI supports 256 busses, 32 slots, 8 functions, and 64 dwords
    // PCIe supports 16777216 busses, 32 slots, 8 functions, 1024 dwords
    // PCIe organizes the busses into up to 65536 segments of 256 busses

    //        43       28 27   20 19  15 14  12 11       2 1  0
    //       +-----------+-------+------+------+----------+----+
    //  PCIe |  segment  |  bus  | slot | func |   dword  |byte|
    //       +-----------+-------+------+------+----------+----+
    //            16         8       5      3       10       2
    //
    //                    27   20 19  15 14  12 .. 7     2 1  0
    //                   +-------+------+------+--+-------+----+
    //  PCI              |  bus  | slot | func |xx| dword |byte|
    //                   +-------+------+------+--+-------+----+
    //                       8       5      3       10       2
    //
    //                        31       16 15    8 7    3 2    0
    //                       +-----------+-------+------+------+
    //  addr                 |  segment  |  bus  | slot | func |
    //                       +-----------+-------+------+------+
    //                            16         8       5      3

    pci_addr_t()
        : addr(0)
    {
    }

    pci_addr_t(int seg, int bus, int slot, int func)
        : addr((uint32_t(seg) << 16) | (bus << 8) | (slot << 3) | (func))
    {
        assert(seg >= 0);
        assert(seg < 65536);
        assert(bus >= 0);
        assert(bus < 256);
        assert(slot >= 0);
        assert(slot < 32);
        assert(func >= 0);
        assert(func < 8);
    }

    int bus() const
    {
        return (addr >> 8) & 0xFF;
    }

    int slot() const
    {
        return (addr >> 3) & 0x1F;
    }

    int func() const
    {
        return addr & 0x7;
    }

    // Returns true if segment is zero
    bool is_legacy() const
    {
        return (addr < 65536);
    }

    uint64_t get_addr() const
    {
        return addr << 12;
    }

private:
    uint32_t addr;
};

struct pci_config_hdr_t {
    uint16_t vendor;
    uint16_t device;

    uint16_t command;
    uint16_t status;

    uint8_t revision;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t dev_class;

    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;

    uint32_t base_addr[6];

    uint32_t cardbus_cis_ptr;

    uint16_t subsystem_vendor;
    uint16_t subsystem_id;

    uint32_t expansion_rom_addr;
    uint8_t capabilities_ptr;

    uint8_t reserved[7];

    uint8_t irq_line;
    uint8_t irq_pin;
    uint8_t min_grant;
    uint8_t max_latency;

    bool is_bar_mmio(ptrdiff_t bar) const
    {
        return (base_addr[bar] & 1) == 0;
    }

    bool is_bar_portio(ptrdiff_t bar) const
    {
        return base_addr[bar] & 1;
    }

    bool is_bar_prefetchable(ptrdiff_t bar) const
    {
        return base_addr[bar] & 8;
    }

    // Returns true if the BAR is MMIO and is 64 bit
    bool is_bar_64bit(ptrdiff_t bar) const
    {
        return (base_addr[bar] & 7) == 4;
    }

    uint64_t get_bar(ptrdiff_t bar) const;

    // Write the specified address to the BAR and read it back, updating
    // config.base_addr[bar] and config.base_addr[bar+1] if it is 64 bit
    void set_mmio_bar(pci_addr_t pci_addr, ptrdiff_t bar, uint64_t addr);
};

#define PCI_DEV_CLASS_UNCLASSIFIED      0x00
#define PCI_DEV_CLASS_STORAGE           0x01
#define PCI_DEV_CLASS_NETWORK           0x02
#define PCI_DEV_CLASS_DISPLAY           0x03
#define PCI_DEV_CLASS_MULTIMEDIA        0x04
#define PCI_DEV_CLASS_MEMORY            0x05
#define PCI_DEV_CLASS_BRIDGE            0x06
#define PCI_DEV_CLASS_COMM              0x07
#define PCI_DEV_CLASS_SYSTEM            0x08
#define PCI_DEV_CLASS_INPUT             0x09
#define PCI_DEV_CLASS_DOCKING           0x0A
#define PCI_DEV_CLASS_PROCESSOR         0x0B
#define PCI_DEV_CLASS_SERIAL            0x0C
#define PCI_DEV_CLASS_WIRELESS          0x0D
#define PCI_DEV_CLASS_INTELLIGENT       0x0E
#define PCI_DEV_CLASS_SATELLITE         0x0F
#define PCI_DEV_CLASS_ENCRYPTION        0x10
#define PCI_DEV_CLASS_DSP               0x11
#define PCI_DEV_CLASS_ACCELERATOR       0x12
#define PCI_DEV_CLASS_INSTRUMENTATION   0x13
#define PCI_DEV_CLASS_COPROCESSOR       0x40
#define PCI_DEV_CLASS_UNASSIGNED        0xFF

// PCI_DEV_CLASS_UNCLASSIFIED
#define PCI_SUBCLASS_UNCLASSIFIED_OLD   0x00
#define PCI_SUBCLASS_UNCLASSIFIED_VGA   0x01

// PCI_DEV_CLASS_STORAGE
#define PCI_SUBCLASS_STORAGE_SCSI       0x00
#define PCI_SUBCLASS_STORAGE_IDE        0x01
#define PCI_SUBCLASS_STORAGE_FLOPPY     0x02
#define PCI_SUBCLASS_STORAGE_IPIBUS     0x03
#define PCI_SUBCLASS_STORAGE_RAID       0x04
#define PCI_SUBCLASS_STORAGE_ATA        0x05
#define PCI_SUBCLASS_STORAGE_SATA       0x06
#define PCI_SUBCLASS_STORAGE_SAS        0x07
#define PCI_SUBCLASS_STORAGE_NVM        0x08
#define PCI_SUBCLASS_STORAGE_MASS       0x80

// PCI_SUBCLASS_STORAGE_SATA
#define PCI_PROGIF_STORAGE_SATA_VEND    0x00
#define PCI_PROGIF_STORAGE_SATA_AHCI    0x01
#define PCI_PROGIF_STORAGE_SATA_SERIAL  0x02

// PCI_SUBCLASS_STORAGE_NVM
#define PCI_PROGIF_STORAGE_NVM_NVME		0x02

// PCI_DEV_CLASS_NETWORK
#define PCI_SUBCLASS_NETWORK_ETHERNET   0x00
#define PCI_SUBCLASS_NETWORK_TOKENRING  0x01
#define PCI_SUBCLASS_NETWORK_FDDI       0x02
#define PCI_SUBCLASS_NETWORK_ATM        0x03
#define PCI_SUBCLASS_NETWORK_ISDN       0x04
#define PCI_SUBCLASS_NETWORK_WFLIP      0x05
#define PCI_SUBCLASS_NETWORK_PICMGMC    0x06
#define PCI_SUBCLASS_NETWORK_OTHER      0x80

// PCI_DEV_CLASS_DISPLAY
#define PCI_SUBCLASS_DISPLAY_VGA        0x00
#define PCI_SUBCLASS_DISPLAY_XGA        0x01
#define PCI_SUBCLASS_DISPLAY_3D         0x02
#define PCI_SUBCLASS_DISPLAY_OTHER      0x80

// PCI_SUBCLASS_DISPLAY_VGA
#define PCI_PROGIF_DISPLAY_VGA_STD      0x00
#define PCI_PROGIF_DISPLAY_VGA_8514     0x01

// PCI_DEV_CLASS_MULTIMEDIA
#define PCI_SUBCLASS_MULTIMEDIA_VIDEO   0x00
#define PCI_SUBCLASS_MULTIMEDIA_AUDIO   0x01
#define PCI_SUBCLASS_MULTIMEDIA_TELEP   0x02
#define PCI_SUBCLASS_MULTIMEDIA_OTHER   0x80

// PCI_DEV_CLASS_MEMORY
#define PCI_SUBCLASS_MEMORY_RAM         0x00
#define PCI_SUBCLASS_MEMORY_FLASH       0x01
#define PCI_SUBCLASS_MEMORY_OTHER       0x80

// PCI_DEV_CLASS_BRIDGE
#define PCI_SUBCLASS_BRIDGE_HOST        0x00
#define PCI_SUBCLASS_BRIDGE_ISA         0x01
#define PCI_SUBCLASS_BRIDGE_EISA        0x02
#define PCI_SUBCLASS_BRIDGE_MCA         0x03
#define PCI_SUBCLASS_BRIDGE_PCI2PCI     0x04
#define PCI_SUBCLASS_BRIDGE_PCMCIA      0x05
#define PCI_SUBCLASS_BRIDGE_NUBUS       0x06
#define PCI_SUBCLASS_BRIDGE_CARDBUS     0x07
#define PCI_SUBCLASS_BRIDGE_RACEWAY     0x08
#define PCI_SUBCLASS_BRIDGE_SEMITP2P    0x09
#define PCI_SUBCLASS_BRIDGE_INFINITI    0x0A
#define PCI_SUBCLASS_BRIDGE_OTHER       0x80

// PCI_SUBCLASS_BRIDGE_PCI2PCI
#define PCI_SUBCLASS_PCI2PCI_NORMAL     0x00
#define PCI_SUBCLASS_PCI2PCI_SUBTRAC    0x01

// PCI_SUBCLASS_BRIDGE_SEMITP2P
#define PCI_SUBCLASS_BRIDGE_SEMITP2P_P  0x40
#define PCI_SUBCLASS_BRIDGE_SEMITP2P_S  0x80

// PCI_DEV_CLASS_COMM
#define PCI_SUBCLASS_COMM_16x50         0x00
#define PCI_SUBCLASS_COMM_PARALLEL      0x01
#define PCI_SUBCLASS_COMM_MULTIPORT     0x02
#define PCI_SUBCLASS_COMM_MODEM         0x03
#define PCI_SUBCLASS_COMM_GPIB          0x04
#define PCI_SUBCLASS_COMM_SMARTCARD     0x05
#define PCI_SUBCLASS_COMM_OTHER         0x80

// PCI_SUBCLASS_COMM_16x50
#define PCI_PROGIF_COMM_16x50_XT        0x00
#define PCI_PROGIF_COMM_16x50_16450     0x01
#define PCI_PROGIF_COMM_16x50_16550     0x02
#define PCI_PROGIF_COMM_16x50_16650     0x03
#define PCI_PROGIF_COMM_16x50_16750     0x04
#define PCI_PROGIF_COMM_16x50_16850     0x05
#define PCI_PROGIF_COMM_16x50_16960     0x06

// PCI_SUBCLASS_COMM_PARALLEL
#define PCI_PROGIF_COMM_PARALLEL_BASIC  0x00
#define PCI_PROGIF_COMM_PARALLEL_BIDIR  0x01
#define PCI_PROGIF_COMM_PARALLEL_ECP    0x02
#define PCI_PROGIF_COMM_PARALLEL_1284   0x03
#define PCI_PROGIF_COMM_PARALLEL_1284D  0xFE

// PCI_SUBCLASS_COMM_MODEM
#define PCI_PROGIF_COMM_MODEM_GENERIC   0x00
#define PCI_PROGIF_COMM_MODEM_HAYES_450 0x01
#define PCI_PROGIF_COMM_MODEM_HAYES_550 0x02
#define PCI_PROGIF_COMM_MODEM_HAYES_650 0x03
#define PCI_PROGIF_COMM_MODEM_HAYES_750 0x04

// PCI_DEV_CLASS_SYSTEM
#define PCI_SUBCLASS_SYSTEM_PIC         0x00
#define PCI_SUBCLASS_SYSTEM_DMA         0x01
#define PCI_SUBCLASS_SYSTEM_TIMER       0x02
#define PCI_SUBCLASS_SYSTEM_RTC         0x03
#define PCI_SUBCLASS_SYSTEM_HOTPLUG     0x04
#define PCI_SUBCLASS_SYSTEM_SDHOST      0x05
#define PCI_SUBCLASS_SYSTEM_OTHER       0x80

// PCI_SUBCLASS_SYSTEM_PIC
#define PCI_PROGIF_SYSTEM_PIC_8259      0x00
#define PCI_PROGIF_SYSTEM_PIC_ISA       0x01
#define PCI_PROGIF_SYSTEM_PIC_EISA      0x02
#define PCI_PROGIF_SYSTEM_PIC_IOAPIC    0x10
#define PCI_PROGIF_SYSTEM_PIC_IOXAPIC   0x20

// PCI_SUBCLASS_SYSTEM_DMA
#define PCI_PROGIF_SYSTEM_DMA_8237      0x00
#define PCI_PROGIF_SYSTEM_DMA_ISA       0x01
#define PCI_PROGIF_SYSTEM_DMA_EISA      0x02

// PCI_SUBCLASS_SYSTEM_TIMER
#define PCI_PROGIF_SYSTEM_TIMER_8254    0x00
#define PCI_PROGIF_SYSTEM_TIMER_ISA     0x01
#define PCI_PROGIF_SYSTEM_TIMER_EISA    0x02

// PCI_SUBCLASS_SYSTEM_RTC
#define PCI_PROGIF_SYSTEM_RTC_GENERIC   0x00
#define PCI_PROGIF_SYSTEM_RTC_ISA       0x01

// PCI_DEV_CLASS_INPUT
#define PCI_SUBCLASS_INPUT_KEYBOARD     0x00
#define PCI_SUBCLASS_INPUT_DIGIPEN      0x01
#define PCI_SUBCLASS_INPUT_MOUSE        0x02
#define PCI_SUBCLASS_INPUT_SCANNER      0x03
#define PCI_SUBCLASS_INPUT_GAME         0x04
#define PCI_SUBCLASS_INPUT_OTHER        0x80

// PCI_SUBCLASS_INPUT_GAME
#define PCI_PROGIF_INPUT_GAME_GENERIC   0x00
#define PCI_PROGIF_INPUT_GAME_STD       0x10

// PCI_DEV_CLASS_DOCKING
#define PCI_SUBCLASS_DOCKING_GENERIC    0x00
#define PCI_SUBCLASS_DOCKING_OTHER      0x80

// PCI_DEV_CLASS_PROCESSOR
#define PCI_SUBCLASS_PROCESSOR_386      0x00
#define PCI_SUBCLASS_PROCESSOR_486      0x01
#define PCI_SUBCLASS_PROCESSOR_PENTIUM  0x02
#define PCI_SUBCLASS_PROCESSOR_ALPHA    0x10
#define PCI_SUBCLASS_PROCESSOR_PPC      0x20
#define PCI_SUBCLASS_PROCESSOR_MIPS     0x30
#define PCI_SUBCLASS_PROCESSOR_COPROC   0x40

// PCI_DEV_CLASS_SERIAL
#define PCI_SUBCLASS_SERIAL_IEEE1394    0x00
#define PCI_SUBCLASS_SERIAL_ACCESS      0x01
#define PCI_SUBCLASS_SERIAL_SSA         0x02
#define PCI_SUBCLASS_SERIAL_USB         0x03
#define PCI_SUBCLASS_SERIAL_FIBRECHAN   0x04
#define PCI_SUBCLASS_SERIAL_SMBUS       0x05
#define PCI_SUBCLASS_SERIAL_INFINIBAND  0x06
#define PCI_SUBCLASS_SERIAL_IPMI        0x07
#define PCI_SUBCLASS_SERIAL_SERCOS      0x08
#define PCI_SUBCLASS_SERIAL_CANBUS      0x09

// PCI_SUBCLASS_SERIAL_IEEE1394
#define PCI_PROGIF_SERIAL_IEEE1394_FW   0x00
#define PCI_PROGIF_SERIAL_IEEE1394_FW   0x00

// PCI_SUBCLASS_SERIAL_USB
#define PCI_PROGIF_SERIAL_USB_UHCI      0x00
#define PCI_PROGIF_SERIAL_USB_OHCI      0x10
#define PCI_PROGIF_SERIAL_USB_EHCI      0x20
#define PCI_PROGIF_SERIAL_USB_XHCI      0x30
#define PCI_PROGIF_SERIAL_USB_UNSPEC    0x80
#define PCI_PROGIF_SERIAL_USB_USBDEV    0xFE

// PCI_SUBCLASS_SERIAL_IPMISMIC
#define PCI_PROGIF_SERIAL_IPMI_SMIC     0x00
#define PCI_PROGIF_SERIAL_IPMI_KEYBD    0x01
#define PCI_PROGIF_SERIAL_IPMI_BLOCK    0x02

// ?PCI_DEV_CLASS_WIRELESS
#define PCI_SUBCLASS_WIRELESS_IRDA      0x00
#define PCI_SUBCLASS_WIRELESS_IR        0x01
#define PCI_SUBCLASS_WIRELESS_RF        0x10
#define PCI_SUBCLASS_WIRELESS_BLUETOOTH 0x11
#define PCI_SUBCLASS_WIRELESS_BROADBAND 0x12
#define PCI_SUBCLASS_WIRELESS_ETH5GHz   0x20
#define PCI_SUBCLASS_WIRELESS_ETH2GHz   0x21
#define PCI_SUBCLASS_WIRELESS_OTHER     0x80

// PCI_DEV_CLASS_INTELLIGENT
#define PCI_SUBCLASS_INTELLIGENT_I2O    0x00
#define PCI_SUBCLASS_INTELLIGENT_FIFO   0x00

// PCI_DEV_CLASS_SATELLITE
#define PCI_SUBCLASS_SATELLITE_TV       0x01
#define PCI_SUBCLASS_SATELLITE_AUDIO    0x02
#define PCI_SUBCLASS_SATELLITE_VOICE    0x03
#define PCI_SUBCLASS_SATELLITE_DATA     0x04

// ?PCI_DEV_CLASS_ENCRYPTION
#define PCI_SUBCLASS_ENCRYPTION_NET     0x00
#define PCI_SUBCLASS_ENCRYPTION_ENTAIN  0x10
#define PCI_SUBCLASS_ENCRYPTION_OTHER   0x80

// PCI_DEV_CLASS_DSP
#define PCI_SUBCLASS_DSP_DPIO           0x00
#define PCI_SUBCLASS_DSP_PERFCNT        0x01
#define PCI_SUBCLASS_DSP_COMMSYNC       0x10
#define PCI_SUBCLASS_DSP_MGMTCARD       0x20
#define PCI_SUBCLASS_DSP_OTHER          0x80

C_ASSERT(offsetof(pci_config_hdr_t, capabilities_ptr) == 0x34);

#define PCI_CFG_STATUS_CAPLIST_BIT  4
#define PCI_CFG_STATUS_CAPLIST      (1<<PCI_CFG_STATUS_CAPLIST_BIT)

struct pci_dev_t {
    pci_config_hdr_t config;
    pci_addr_t addr;
};

struct pci_dev_iterator_t : public pci_dev_t {
    operator pci_addr_t() const
    {
        return pci_addr_t(segment, bus, slot, func);
    }

    int segment;
    int bus;
    int slot;
    int func;

    int dev_class;
    int subclass;
    int vendor;
    int device;

    uint8_t header_type;

    uint8_t bus_todo_len;
    uint8_t bus_todo[64];

    void reset()
    {
        segment = 0;
        bus = 0;
        slot = 0;
        func = 0;
        dev_class = 0;
        subclass = 0;
        vendor = 0;
        device = 0;
        header_type = 0;
        bus_todo_len = 0;
        memset(bus_todo, 0, sizeof(bus_todo));
    }

    pci_dev_iterator_t& copy_from(pci_dev_iterator_t const& rhs)
    {
        segment = rhs.segment;
        bus = rhs.bus;
        slot = rhs.slot;
        func = rhs.func;

        config = rhs.config;
        addr = pci_addr_t(segment, bus, slot, func);
        return *this;
    }

    bool operator==(pci_dev_iterator_t const& rhs) const
    {
        return (segment == rhs.segment) &
                (bus == rhs.bus) &
                (slot == rhs.slot) &
                (func == rhs.func);
    }
};

int pci_init(void);

int pci_enumerate_begin(pci_dev_iterator_t *iter,
                        int dev_class = -1, int subclass = -1,
                        int vendor = -1, int device = -1);
int pci_enumerate_next(pci_dev_iterator_t *iter);

uint32_t pci_config_read(pci_addr_t addr, int offset, int size);

bool pci_config_write(pci_addr_t addr,
                      size_t offset, void *values, size_t size);

void pci_config_copy(pci_addr_t addr, void *dest, int ofs, size_t size);

// Returns positive count for msix, or negated count for msi
int pci_max_vectors(pci_addr_t addr);

int pci_find_capability(pci_addr_t addr,
        int capability_id, int start = 0);

int pci_enum_capabilities(int start, pci_addr_t addr,
                          int (*callback)(uint8_t, int, uintptr_t),
                          uintptr_t context);

//
// PCI capability IDs

// Power management
#define PCICAP_PM       0x1

// Accelerated Graphics Port
#define PCICAP_AGP      0x2

// Vital Product Data
#define PCICAP_VPD      0x3

// Slot Identification (external expansion)
#define PCICAP_SLOTID   0x4

// Message Signaled Interrupts
#define PCICAP_MSI      0x5

// CompactPCI Hotswap
#define PCICAP_HOTSWAP  0x6

// PCI-X
#define PCICAP_PCIX     0x7

// HyperTransport
#define PCICAP_HTT      0x8

// Vendor specific
#define PCICAP_VENDOR   0x9

// Debug port
#define PCICAP_DEBUG    0xA

// CompactPCI central resource control
#define PCICAP_CPCI     0xB

// PCI Hot-plug
#define PCICAP_HOTPLUG  0xC

// PCI bridge subsystem vendor ID
#define PCICAP_BRID     0xD

// AGP 8x
#define PCICAP_AGP8x    0xE

// Secure device
#define PCICAP_SECDEV   0xF

// Vendor specific
#define PCICAP_VENDOR   0x9

// PCI Express
#define PCICAP_PCIE     0x10

// MSI-X
#define PCICAP_MSIX     0x11

struct pci_irq_range_t {
    uint8_t base;
    uint8_t count:7;
    bool msix:1;
} _packed;

void pci_init_ecam(size_t ecam_count);
void pci_init_ecam_entry(uint64_t base, uint16_t seg,
                         uint8_t st_bus, uint8_t en_bus);
void pci_init_ecam_enable();

bool pci_try_msi_irq(pci_dev_iterator_t const& pci_dev,
                     pci_irq_range_t *irq_range,
                     int cpu, bool distribute, int req_count,
                     intr_handler_t handler, char const *name,
                     int const *target_cpus = nullptr,
                     int const *vector_offsets = nullptr);

bool pci_set_msi_irq(pci_addr_t addr,
                     pci_irq_range_t *irq_range,
                     int cpu, bool distribute, int req_count,
                     intr_handler_t handler, char const *name,
                     int const *target_cpus = nullptr,
                     int const *vector_offsets = nullptr);

bool pci_set_irq_unmask(pci_addr_t addr,
                        bool unmask);

void pci_set_irq_line(pci_addr_t addr, uint8_t irq_line);

void pci_set_irq_pin(pci_addr_t addr, uint8_t irq_pin);

void pci_adj_control_bits(pci_dev_iterator_t const& pci_dev,
                          uint16_t set, uint16_t clr);

void pci_adj_control_bits(int bus, int slot, int func,
                          uint16_t set, uint16_t clr);

void pci_clear_status_bits(pci_addr_t addr, uint16_t bits);

char const * pci_device_class_text(uint8_t cls);

int pci_vector_count_from_offsets(int const *vector_offsets, int count);

// == MMIO accessor helpers

#define _MM_WR_IMPL(type, mm, value) \
    __asm__ __volatile__ ( \
        "mov %" type "[src],(%q[dest])\n\t" \
        : \
        : [dest] "a" (&(mm)) \
        , [src] "ri" (value) \
        : "memory" \
    )

#define _MM_RD_IMPL(type, value, mm) \
    __asm__ __volatile__ ( \
        "mov (%[src]),%" type "[dest]\n\t" \
        : [dest] "=r" (value) \
        : [src] "a" (&(mm)) \
        : "memory" \
    ); \
    return value

static inline void mm_wr(uint8_t volatile& dest, uint8_t src)
{
    _MM_WR_IMPL("b", dest, src);
}

static inline void mm_wr(uint16_t volatile& dest, uint16_t src)
{
    _MM_WR_IMPL("w", dest, src);
}

static inline void mm_wr(uint32_t volatile& dest, uint32_t src)
{
    _MM_WR_IMPL("k", dest, src);
}

static inline void mm_wr(uint64_t volatile& dest, uint64_t src)
{
    _MM_WR_IMPL("q", dest, src);
}

static inline void mm_wr(int8_t volatile& dest, int8_t src)
{
    _MM_WR_IMPL("b", dest, src);
}

static inline void mm_wr(int16_t volatile& dest, int16_t src)
{
    _MM_WR_IMPL("w", dest, src);
}

static inline void mm_wr(int32_t volatile& dest, int32_t src)
{
    _MM_WR_IMPL("k", dest, src);
}

static inline void mm_wr(int64_t volatile& dest, int64_t src)
{
    _MM_WR_IMPL("q", dest, src);
}

static inline void mm_copy_wr(void volatile *dst, void const *src, size_t sz)
{
    while (sz >= sizeof(uint32_t)) {
        mm_wr(*(uint32_t volatile*)dst, *(uint32_t const*)src);
        dst = (char*)dst + sizeof(uint32_t);
        src = (char*)src + sizeof(uint32_t);
        sz -= sizeof(uint32_t);
    }

    while (sz >= sizeof(uint16_t)) {
        mm_wr(*(uint16_t volatile*)dst, *(uint16_t const*)src);
        dst = (char*)dst + sizeof(uint16_t);
        src = (char*)src + sizeof(uint16_t);
        sz -= sizeof(uint16_t);
    }

    while (sz >= sizeof(uint8_t)) {
        mm_wr(*(uint8_t volatile*)dst, *(uint8_t const*)src);
        dst = (char*)dst + sizeof(uint8_t);
        src = (char*)src + sizeof(uint8_t);
        sz -= sizeof(uint8_t);
    }
}

//

static inline uint8_t mm_rd(uint8_t volatile const& src)
{
    uint8_t dest;
    _MM_RD_IMPL("b", dest, src);
}

static inline int8_t mm_rd(int8_t volatile const& src)
{
    uint8_t dest;
    _MM_RD_IMPL("b", dest, src);
}

static inline uint16_t mm_rd(uint16_t volatile const& src)
{
    uint16_t dest;
    _MM_RD_IMPL("w", dest, src);
}

static inline int16_t mm_rd(int16_t volatile const& src)
{
    uint16_t dest;
    _MM_RD_IMPL("w", dest, src);
}

static inline uint32_t mm_rd(uint32_t volatile const& src)
{
    uint32_t dest;
    _MM_RD_IMPL("k", dest, src);
}

static inline int32_t mm_rd(int32_t volatile const& src)
{
    uint32_t dest;
    _MM_RD_IMPL("k", dest, src);
}

static inline uint64_t mm_rd(uint64_t volatile const& src)
{
    uint64_t dest;
    _MM_RD_IMPL("q", dest, src);
}

static inline int64_t mm_rd(int64_t volatile const& src)
{
    uint64_t dest;
    _MM_RD_IMPL("q", dest, src);
}

static inline void mm_copy_rd(void *dst, void const volatile *src, size_t sz)
{
    while (sz >= sizeof(uint32_t)) {
        *(uint32_t*)dst = mm_rd(*(uint32_t const volatile *)src);
        dst = (char*)dst + sizeof(uint32_t);
        src = (char*)src + sizeof(uint32_t);
        sz -= sizeof(uint32_t);
    }

    while (sz >= sizeof(uint16_t)) {
        *(uint16_t*)dst = mm_rd(*(uint16_t const*)src);
        dst = (char*)dst + sizeof(uint16_t);
        src = (char*)src + sizeof(uint16_t);
        sz -= sizeof(uint16_t);
    }

    while (sz >= sizeof(uint8_t)) {
        *(uint8_t*)dst = mm_rd(*(uint8_t const*)src);
        dst = (char*)dst + sizeof(uint8_t);
        src = (char*)src + sizeof(uint8_t);
        sz -= sizeof(uint8_t);
    }
}

#undef _MM_RD_IMPL

struct pci_cache_t {
    std::vector<pci_dev_iterator_t> iters;
    uint64_t updated_at;
};
