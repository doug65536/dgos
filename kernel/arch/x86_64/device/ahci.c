#include "device/ahci.h"
#include "device/pci.h"
#include "printk.h"
#include "mm.h"
#include "assert.h"

typedef enum ahci_fis_type_t {
    // Register FIS - host to device
    FIS_TYPE_REG_H2D	= 0x27,

    // Register FIS - device to host
    FIS_TYPE_REG_D2H	= 0x34,

    // DMA activate FIS - device to host
    FIS_TYPE_DMA_ACT	= 0x39,

    // DMA setup FIS - bidirectional
    FIS_TYPE_DMA_SETUP	= 0x41,

    // Data FIS - bidirectional
    FIS_TYPE_DATA		= 0x46,

    // BIST activate FIS - bidirectional
    FIS_TYPE_BIST		= 0x58,

    // PIO setup FIS - device to host
    FIS_TYPE_PIO_SETUP	= 0x5F,

    // Set device bits FIS - device to host
    FIS_TYPE_DEV_BITS	= 0xA1,
} ahci_fis_type_t;

typedef struct ahci_fis_h2d_t {
    // FIS_TYPE_REG_H2D
    uint8_t	fis_type;

    // PORTMUX and CMD
    uint8_t	ctl;

    // Command register
    uint8_t	command;

    // Feature register, 7:0
    uint8_t	feature_lo;


    // LBA low register, 7:0
    uint8_t	lba0;

    // LBA mid register, 15:8
    uint8_t	lba1;

    // LBA high register, 23:16
    uint8_t	lba2;

    // Device register
    uint8_t	device;


    // LBA register, 31:24
    uint8_t	lba3;

    // LBA register, 39:32
    uint8_t	lba4;

    // LBA register, 47:40
    uint8_t	lba5;

    // Feature register, 15:8
    uint8_t	feature_hi;

    // Count register, 15:0
    uint16_t	count;

    // Isochronous command completion
    uint8_t	icc;

    // Control register
    uint8_t	control;

    // Reserved
    uint8_t	rsv1[4];
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
    uint8_t	fis_type;

    // PORTMUX and INTR
    uint8_t	ctl;

    // Status
    uint8_t	status;

    // Error
    uint8_t	error;

    // LBA 15:0
    uint16_t lba0;

    // LBA 23:16
    uint8_t	lba2;

    // Device
    uint8_t	device;

    // LBA 39:24
    uint16_t lba3;

    // LBA 47:40
    uint8_t	lba5;

    // Reserved
    uint8_t	rsv2;

    // Count 15:0
    uint16_t	count;

    uint8_t	rsv3[2];    // Reserved

    uint32_t rsv4;    // Reserved
} ahci_fis_d2h_t;

#define AHCI_FIS_CTL_INTR_BIT   1
#define AHCI_FIS_CTL_INTR       (1<<AHCI_FIS_CTL_INTR_BIT)

typedef struct tagFIS_DMA_SETUP
{
    // FIS_TYPE_DMA_SETUP
    uint8_t	fis_type;

    // PORTMUX, DIR, INTR, AUTO
    uint8_t	ctl;

    // Reserved
    uint16_t    rsv;

    // DMA Buffer Identifier. Used to Identify DMA buffer in host memory.
    // SATA Spec says host specific and not in Spec.
    // Trying AHCI spec might work.
    uint64_t   DMAbufferID;

    // Reserved
    uint32_t   rsv2;

    // Byte offset into buffer. First 2 bits must be 0
    uint32_t   DMAbufOffset;

    //Number of bytes to transfer. Bit 0 must be 0
    uint32_t   TransferCount;

    //Reserved
    uint32_t   rsv3;
} FIS_DMA_SETUP;

#define AHCI_FIS_CTL_DIR_BIT    2
#define AHCI_FIS_CTL_AUTO_BIT   0

#define AHCI_FIS_CTL_DIR        (1<<AHCI_FIS_CTL_DIR_BIT)
#define AHCI_FIS_AUTO_DIR        (1<<AHCI_FIS_CTL_AUTO_BIT)

typedef struct ahci_dev_t {
    pci_config_hdr_t config;
    char volatile *mmio_base;
} ahci_dev_t;

#define AHCI_MAX_DEVICES    16
static ahci_dev_t ahci_devices[AHCI_MAX_DEVICES];
static unsigned ahci_count;

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

            if (ahci_count < countof(ahci_devices)) {
                ahci_devices[ahci_count].config = pci_iter.config;

                ahci_devices[ahci_count].mmio_base =
                        mmap((void*)(uintptr_t)pci_iter.config.base_addr[5],
                        0x1100, PROT_READ | PROT_WRITE,
                        MAP_PHYSICAL, -1, 0);

                ++ahci_count;
            }
        }
        printk("\nIRQ line=%d, IRQ pin=%d\n",
               pci_iter.config.irq_line,
               pci_iter.config.irq_pin);
    } while (pci_enumerate_next(&pci_iter));
}
