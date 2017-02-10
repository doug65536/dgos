#include "rtl8139.h"
#include "pci.h"
#include "callout.h"
#include "printk.h"
#include "cpu/ioport.h"
#include "stdlib.h"
#include "cpu/atomic.h"

typedef struct rtl8139_dev_t rtl8139_dev_t;
struct rtl8139_dev_t {
    void *vtbl;

    ioport_t io_base;
};

// MAC address (must be 32-bit I/O)
#define RTL8139_IO_IDR_LO       0x00
#define RTL8139_IO_IDR_HI       0x04

// Multicast Address Register (must be 32-bit I/O)
#define RTL8139_IO_MAR_LO       0x08
#define RTL8139_IO_MAR_HI       0x0C

// Transmit status descriptor 0-3 (32-bit)
#define RTL8139_IO_TSD_n(n)     (0x10 + ((n) << 2)

// Transmit start address descriptor 0-3 (32-bit)
#define RTL8139_IO_TSAD_n(n)    (0x20 + ((n) << 2)

// Transmit buffer address descriptor (32-bit)
#define RTL8139_IO_RBSTART      0x30

// Early receive byte count (16-bit)
#define RTL8139_IO_ERBCR        0x34

// Early receive status register (8-bit)
#define RTL8139_IO_ERSR         0x36

// Command register (8-bit)
#define RTL8139_IO_CR           0x37

// Current address of packet read (16-bit)
#define RTL8139_IO_CAPR         0x38

// Current receive buffer address (16-bit)
#define RTL8139_IO_CBR          0x3A

// Interrupt Mask Register (16-bit)
#define RTL8139_IO_IMR          0x3C

// Interrupt Status Register (16-bit)
#define RTL8139_IO_ISR          0x3E

// Transmit Configuration Register (32-bit)
#define RTL8139_IO_TCR          0x40

// Receive Configuration Register (32-bit)
#define RTL8139_IO_RCR          0x44

// Timer Count Register (32-bit)
#define RTL8139_IO_TCTR         0x48

// Missed Packet Counter (32-bit)
#define RTL8139_IO_MPC          0x4C

// 93C46 Command Register (8-bit)
#define RTL8139_IO_9346CR       0x50

// Configuration Register 0 (8-bit)
#define RTL8139_IO_CONFIG0      0x51

// Configuration Register 1 (8-bit)
#define RTL8139_IO_CONFIG1      0x52

// Timer Interrupt Register (32-bit)
#define RTL8139_IO_TIMERINT     0x54

// Media Status Register (8-bit)
#define RTL8139_IO_MSR          0x58

// Configuration Register 3 (8-bit)
#define RTL8139_IO_CONFIG3      0x59

// Configuration Register 4 (8-bit)
#define RTL8139_IO_CONFIG4      0x5A

// Multiple Interrupt Select (16-bit)
#define RTL8139_IO_MULINT       0x5C

// PCI Revision ID (8-bit)
#define RTL8139_IO_RERID        0x5E

// Transmit Status of All Descriptors (16-bit)
#define RTL8139_IO_TSAD         0x60

// Basic Mode Control Register (16-bit)
#define RTL8139_IO_BMCR         0x62

// Basic Mode Status Register (16-bit)
#define RTL8139_IO_BMSR         0x64

// Auto-Negotiation Address Register (16-bit)
#define RTL8139_IO_ANAR         0x66

// Auto-Negotiation Link Partner Register (16-bit)
#define RTL8139_IO_ANLPAR       0x68

// Auto-Negotiation Expansion Register (16-bit)
#define RTL8139_IO_ANER         0x6A

// Disconnect counter (16-bit)
#define RTL8139_IO_DIS          0x6C

// False Carrier Sense Counter (16-bit)
#define RTL8139_IO_FCSC         0x6E

// N-Way Test Register (16-bit)
#define RTL8139_IO_NWAYTR       0x70

// RX_ER Counter (16-bit)
#define RTL8139_IO_REC          0x72

// CS Configuration Register (16-bit)
#define RTL8139_IO_CSCR         0x74

// PHY Parameter 1 (32-bit)
#define RTL8139_IO_PHY1_PARM    0x78

// Twister Parameter (32-bit)
#define RTL8139_IO_TW_PARM      0x7C

// PHY Parameter 2 (8-bit)
#define RTL8139_IO_PHY2_PARM    0x80

// Power Management CRC register for wakeup frame n (8-bit)
#define RTL8139_IO_CRC_n(n)     (0x84 + (n))

// Power Management wakeup frame0 (64-bit)
#define RTL8139_IO_WAKELO_n(n)  (0x8C + ((n)<<2))
#define RTL8139_IO_WAKEHI_n(n)  (0x90 + ((n)<<2))

// LSB of mask byte of wakeup frame n within offset 12 to 75 (8-bit)
#define RTL8139_IO_LSBCRC0_n(n) (0xCC + (n))

static void rtl8139_detect(void)
{
    pci_dev_iterator_t pci_iter;

    pci_enumerate_begin(&pci_iter,
                        PCI_DEV_CLASS_NETWORK,
                        PCI_SUBCLASS_NETWORK_ETHERNET);

    do {
        if (pci_iter.config.vendor != 0x10EC ||
                pci_iter.config.device != 0x8139)
            continue;

        printdbg("Detected RTL8139 %d/%d/%d"
                 " %x %x %x %x %x %x irq=%d\n",
                 pci_iter.bus,
                 pci_iter.slot,
                 pci_iter.func,
                 pci_iter.config.base_addr[0],
                pci_iter.config.base_addr[1],
                pci_iter.config.base_addr[2],
                pci_iter.config.base_addr[3],
                pci_iter.config.base_addr[4],
                pci_iter.config.base_addr[5],
                pci_iter.config.irq_line);

        // Enable I/O and bus master
        pci_adj_control_bits(pci_iter.bus, pci_iter.slot,
                             pci_iter.func,
                             PCI_CMD_BUSMASTER | PCI_CMD_IOEN, 0);

        ioport_t port_base = pci_iter.config.base_addr[0] & -4;

        // Reset
        outb(port_base + RTL8139_IO_CR, 0x10);

        while ((inb(port_base + RTL8139_IO_CR) & 0x10) != 0)
            pause();


    } while (pci_enumerate_next(&pci_iter));
}

static void rtl8139_startup_hack(void *p)
{
    (void)p;
    rtl8139_detect();
}

REGISTER_CALLOUT(rtl8139_startup_hack, 0, 'L', "000");
