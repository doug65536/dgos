#include "pci.h"
#include "callout.h"
#include "printk.h"
#include "stdlib.h"
#include "cpu/atomic.h"
#include "mm.h"
#include "irq.h"
#include "mutex.h"
#include "string.h"
#include "bswap.h"
#include "eth_q.h"
#include "eth_frame.h"
#include "time.h"
#include "udp_frame.h"
#include "dev_eth.h"

// test
#include "net/dhcp.h"

#define RTL8139_DEBUG   1
#if RTL8139_DEBUG
#define RTL8139_TRACE(...) printdbg("RTL8139: " __VA_ARGS__)
#else
#define RTL8139_TRACE(...) (void)0
#endif

//
// MMIO

#define RTL8139_MMIO_PTR(type, reg) \
    (*(type volatile *)(((char*)mmio) + (reg)))

#define RTL8139_MMIO(type, reg)     RTL8139_MMIO_PTR(type, reg)

#define RTL8139_MM_WR_8(reg, val) \
    rtl8139_mm_out_8(reg, val)

#define RTL8139_MM_WR_16(reg, val) \
    rtl8139_mm_out_16(reg, val)

#define RTL8139_MM_WR_32(reg, val) \
    rtl8139_mm_out_32(reg, val)

#define RTL8139_MM_RD_8(reg) \
    rtl8139_mm_in_8(reg)

#define RTL8139_MM_RD_16(reg) \
    rtl8139_mm_in_16(reg)

#define RTL8139_MM_RD_32(reg) \
    rtl8139_mm_in_32(reg)

struct rtl8139_factory_t : public eth_dev_factory_t {
    rtl8139_factory_t() : eth_dev_factory_t("rtl8139") {}
    virtual int detect(eth_dev_base_t ***result);
};

static rtl8139_factory_t rtl8139_factory;

struct rtl8139_dev_t : public eth_dev_base_t {
    ETH_DEV_IMPL

    __used
    __always_inline void rtl8139_mm_out_8(uint32_t reg, uint8_t val)
    {
        RTL8139_MMIO(uint8_t, reg) = val;
        atomic_barrier();
    }

    __used
    __always_inline void rtl8139_mm_out_16(uint32_t reg, uint16_t val)
    {
        RTL8139_MMIO(uint16_t, reg) = val;
        atomic_barrier();
    }

    __used
    __always_inline void rtl8139_mm_out_32(uint32_t reg, uint32_t val)
    {
        RTL8139_MMIO(uint32_t, reg) = val;
        atomic_barrier();
    }

    __used
    __always_inline uint8_t rtl8139_mm_in_8(uint32_t reg)
    {
        atomic_barrier();
        return RTL8139_MMIO(uint8_t, reg);
    }

    __used
    __always_inline uint16_t rtl8139_mm_in_16(uint32_t reg)
    {
        atomic_barrier();
        return RTL8139_MMIO(uint16_t, reg);
    }

    __used
    __always_inline uint32_t rtl8139_mm_in_32(uint32_t reg)
    {
        atomic_barrier();
        return RTL8139_MMIO(uint32_t, reg);
    }

    void detect(const pci_dev_iterator_t &pci_dev);

    void tx_packet(int slot, ethq_pkt_t *pkt);
    void rx_irq_handler(uint16_t isr);
    void tx_irq_handler();
    void irq_handler();
    static isr_context_t *irq_dispatcher(int irq, isr_context_t *ctx);

    uintptr_t mmio_physaddr;
    void volatile *mmio;

    uintptr_t rx_buffer_physaddr;
    void *rx_buffer;
    size_t rx_offset;

    ethq_pkt_t *tx_pkts[4];
    ethq_pkt_t *tx_next[4];

    unsigned tx_head;
    unsigned tx_tail;

    using lock_type = mcslock;
    using scoped_lock = unique_lock<lock_type>;
    lock_type lock;

    ethq_queue_t tx_queue;
    ethq_queue_t rx_queue;

    // Actually 6 bytes
    uint8_t mac_addr[8];

    // PCI
    bool use_msi;
    pci_irq_range_t irq_range;
};

static rtl8139_dev_t **rtl8139_devices;
static size_t rtl8139_device_count;

// MAC address (must be 32-bit I/O)
#define RTL8139_IO_IDR_HI       0x00
#define RTL8139_IO_IDR_LO       0x04

// Multicast Address Register (must be 32-bit I/O)
#define RTL8139_IO_MAR_LO       0x08
#define RTL8139_IO_MAR_HI       0x0C

// Tx status descriptor 0-3 (32-bit)
#define RTL8139_IO_TSD_n(n)     (0x10 + ((n) << 2))

// Tx start address descriptor 0-3 (32-bit)
#define RTL8139_IO_TSAD_n(n)    (0x20 + ((n) << 2))

// Rx buffer address descriptor (32-bit)
#define RTL8139_IO_RBSTART      0x30

// Early rx byte count (16-bit)
#define RTL8139_IO_ERBCR        0x34

// Early rx status register (8-bit)
#define RTL8139_IO_ERSR         0x36

// Command register (8-bit)
#define RTL8139_IO_CR           0x37

// Current address of packet read (16-bit)
#define RTL8139_IO_CAPR         0x38

// Current rx buffer address (16-bit)
#define RTL8139_IO_CBR          0x3A

// Interrupt Mask Register (16-bit)
#define RTL8139_IO_IMR          0x3C

// Interrupt Status Register (16-bit)
#define RTL8139_IO_ISR          0x3E

// Tx Configuration Register (32-bit)
#define RTL8139_IO_TCR          0x40

// Rx Configuration Register (32-bit)
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

// Tx Status of All Descriptors (16-bit)
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
#define RTL8139_IO_WAKELO_n(n)  (0x8C + ((n) << 2))
#define RTL8139_IO_WAKEHI_n(n)  (0x90 + ((n) << 2))

// LSB of mask byte of wakeup frame n within offset 12 to 75 (8-bit)
#define RTL8139_IO_LSBCRC0_n(n) (0xCC + (n))

//
// Register bit defines

//
// RTL8139_IO_CR: Command register

#define RTL8139_CR_RST_BIT      4
#define RTL8139_CR_RXEN_BIT     3
#define RTL8139_CR_TXEN_BIT     2
#define RTL8139_CR_RXEMPTY_BIT  0

// Reset
#define RTL8139_CR_RST          (1U<<RTL8139_CR_RST_BIT)

// Rx enable
#define RTL8139_CR_RXEN         (1U<<RTL8139_CR_RXEN_BIT)

// Tx enable
#define RTL8139_CR_TXEN         (1U<<RTL8139_CR_TXEN_BIT)

// Rx buffer empty (read only)
#define RTL8139_CR_RXEMPTY      (1U<<RTL8139_CR_RXEMPTY_BIT)

//
// RTL8139_IO_TSD_n: Tx status descriptor 0-3

#define RTL8139_TSD_CRS_BIT     31
#define RTL8139_TSD_TABT_BIT    30
#define RTL8139_TSD_OWC_BIT     29
#define RTL8139_TSD_CDH_BIT     28
#define RTL8139_TSD_NCC_BIT     24
#define RTL8139_TSD_ERTXTH_BIT  16
#define RTL8139_TSD_TOK_BIT     15
#define RTL8139_TSD_TUN_BIT     14
#define RTL8139_TSD_OWN_BIT     13
#define RTL8139_TSD_SIZE_BIT    0

#define RTL8139_TSD_NCC_BITS    4
#define RTL8139_TSD_ERTXTH_BITS 6
#define RTL8139_TSD_SIZE_BITS   13

#define RTL8139_TSD_NCC_MASK    ((1U<<RTL8139_TSD_NCC_BITS)-1)
#define RTL8139_TSD_ERTXTH_MASK ((1U<<RTL8139_TSD_ERTXTH_BITS)-1)
#define RTL8139_TSD_SIZE_MASK   ((1U<<RTL8139_TSD_SIZE_BITS)-1)

// Carrier sense lost (read only)
#define RTL8139_TSD_CRS         (1U<<RTL8139_TSD_CRS_BIT)

// Transmit aborted (read only)
#define RTL8139_TSD_TABT        (1U<<RTL8139_TSD_TABT_BIT)

// Out of window collision (read only)
#define RTL8139_TSD_OWC         (1U<<RTL8139_TSD_OWC_BIT)

// CD Heart Beat (10Mbps only) (read only)
#define RTL8139_TSD_CDH         (1U<<RTL8139_TSD_CDH_BIT)

// Transmit OK (read only)
#define RTL8139_TSD_TOK         (1U<<RTL8139_TSD_TOK_BIT)

// Transmit FIFO underrun (read only)
#define RTL8139_TSD_TUN         (1U<<RTL8139_TSD_TUN_BIT)

// Set to 0 if NIC is using descriptor,
// Set to 1 if ready for driver to provide another packet
// Must write 0 to this bit to start tx
#define RTL8139_TSD_OWN         (1U<<RTL8139_TSD_OWN_BIT)

// Number of tx collisions count
#define RTL8139_TSD_NCC \
    (RTL8139_TSD_NCC_MASK<<RTL8139_TSD_NCC_BIT)

// Early tx threshold (in units of 8 bytes, 0=8 bytes, max 2KB)
#define RTL8139_TSD_ERTXTH \
    (RTL8139_TSD_ERTXTH_MASK<<RTL8139_TSD_ERTXTH_BIT)

// Tx packet size
#define RTL8139_TSD_SIZE \
    (RTL8139_TSD_SIZE_MASK<<RTL8139_TSD_SIZE_BIT)

// Early tx threshold (in units of 8 bytes, 0=8 bytes, max 2KB)
#define RTL8139_TSD_ERTXTH_n(n) ((n)<<RTL8139_TSD_ERTXTH_BIT)

// Tx packet size
#define RTL8139_TSD_SIZE_n(n)   ((n)<<RTL8139_TSD_SIZE_BIT)
#define RTL8139_TSD_SIZE_MAX    1792

//
// RTL8139_IO_IMR and RTL8139_IO_ISR: Interrupt Mask/Status Registers

#define RTL8139_IxR_SERR_BIT        15
#define RTL8139_IxR_TIMEOUT_BIT     14
#define RTL8139_IxR_LENCHG_BIT      13
#define RTL8139_IxR_FOVW_BIT        6
#define RTL8139_IxR_PUNLC_BIT       5
#define RTL8139_IxR_RXOVW_BIT       4
#define RTL8139_IxR_TER_BIT         3
#define RTL8139_IxR_TOK_BIT         2
#define RTL8139_IxR_RER_BIT         1
#define RTL8139_IxR_ROK_BIT         0

// System error
#define RTL8139_IxR_SERR            (1U<<RTL8139_IxR_SERR_BIT)

// Timeout
#define RTL8139_IxR_TIMEOUT         (1U<<RTL8139_IxR_TIMEOUT_BIT)

// Cable length change
#define RTL8139_IxR_LENCHG          (1U<<RTL8139_IxR_LENCHG_BIT)

// Rx FIFO Overflow
#define RTL8139_IxR_FOVW            (1U<<RTL8139_IxR_FOVW_BIT)

// Packet underrun / link change
#define RTL8139_IxR_PUNLC           (1U<<RTL8139_IxR_PUNLC_BIT)

// Rx overflow
#define RTL8139_IxR_RXOVW           (1U<<RTL8139_IxR_RXOVW_BIT)

// Transmit error
#define RTL8139_IxR_TER             (1U<<RTL8139_IxR_TER_BIT)

// Transmit OK
#define RTL8139_IxR_TOK             (1U<<RTL8139_IxR_TOK_BIT)

// Rx error
#define RTL8139_IxR_RER             (1U<<RTL8139_IxR_RER_BIT)

// Rx OK
#define RTL8139_IxR_ROK             (1U<<RTL8139_IxR_ROK_BIT)

//
// RTL8139_IO_RCR: Rx Configuration Register (32-bit)

#define RTL8139_RCR_ERTH_BIT        24
#define RTL8139_RCR_MULERINT_BIT    17
#define RTL8139_RCR_RER8_BIT        16
#define RTL8139_RCR_RXFTH_BIT       13
#define RTL8139_RCR_RBLEN_BIT       11
#define RTL8139_RCR_MXDMA_BIT       8
#define RTL8139_RCR_WRAP_BIT        7
#define RTL8139_RCR_AER_BIT         5
#define RTL8139_RCR_AR_BIT          4
#define RTL8139_RCR_AB_BIT          3
#define RTL8139_RCR_AM_BIT          2
#define RTL8139_RCR_APM_BIT         1
#define RTL8139_RCR_AAP_BIT         0

#define RTL8139_RCR_ERTH_BITS       4
#define RTL8139_RCR_RXFTH_BITS      3
#define RTL8139_RCR_RBLEN_BITS      2
#define RTL8139_RCR_MXDMA_BITS      3

#define RTL8139_RCR_ERTH_MASK       ((1U<<RTL8139_RCR_ERTH_BITS)-1)
#define RTL8139_RCR_RXFTH_MASK      ((1U<<RTL8139_RCR_RXFTH_BITS)-1)
#define RTL8139_RCR_RBLEN_MASK      ((1U<<RTL8139_RCR_RBLEN_BITS)-1)
#define RTL8139_RCR_MXDMA_MASK      ((1U<<RTL8139_RCR_MXDMA_BITS)-1)

#define RTL8139_RCR_ERTH \
    (RTL8139_RCR_ERTH_MASK<<RTL8139_RCR_ERTH_BIT)

#define RTL8139_RCR_RXFTH \
    (RTL8139_RCR_RXFTH_MASK<<RTL8139_RCR_RXFTH_BIT)

#define RTL8139_RCR_RBLEN \
    (RTL8139_RCR_RBLEN_MASK<<RTL8139_RCR_RBLEN_BIT)

#define RTL8139_RCR_MXDMA \
    (RTL8139_RCR_MXDMA_MASK<<RTL8139_RCR_MXDMA_BIT)

// Multiple early interrupt select
// 0 = Invoke early interrupt for unknown
//      protocols using RCR<ERTH[3:0]>
// 1 = Invoke early interrupt for known
//      protocols using MULINT<MISR[11:0]>
#define RTL8139_RCR_MULERINT        (1U<<RTL8139_RCR_MULERINT_BIT)

// 0 = Receive error packets larger than 8 bytes
// 1 = Receive error packets larger than 64 bytes (default)
#define RTL8139_RCR_RER8            (1U<<RTL8139_RCR_RER8_BIT)

// 0 = Wrap at end of buffer,
// 1 = overflow up to 1500 bytes past end
#define RTL8139_RCR_WRAP            (1U<<RTL8139_RCR_WRAP_BIT)

// Accept error packets
#define RTL8139_RCR_AER             (1U<<RTL8139_RCR_AER_BIT)

// Accept runt packets
#define RTL8139_RCR_AR              (1U<<RTL8139_RCR_AR_BIT)

// Accept broadcast packets
#define RTL8139_RCR_AB              (1U<<RTL8139_RCR_AB_BIT)

// Accept multicast packets
#define RTL8139_RCR_AM              (1U<<RTL8139_RCR_AM_BIT)

// Accept physical match packets
#define RTL8139_RCR_APM             (1U<<RTL8139_RCR_APM_BIT)

// Accept all packets
#define RTL8139_RCR_AAP             (1U<<RTL8139_RCR_AAP_BIT)

// Early Rx threshold (n/16th, or, 0=none)
#define RTL8139_RCR_ERTH_n(n)   ((n)<<RTL8139_RCR_ERTH_BIT)

// Rx FIFO threshold (1U<<(n+4), or 7=none)
#define RTL8139_RCR_RXFTH_n(n)  ((n)<<RTL8139_RCR_RXFTH_BIT)

// Rx Buffer Length (0=8K+16, 1=16K+16, 2=32K+16, 3=64K+16)
#define RTL8139_RCR_RBLEN_n(n)  ((n)<<RTL8139_RCR_RBLEN_BIT)

// Max DMA burst per Rx DMA burst (1U<<(n+4), or 7=unlimited)
#define RTL8139_RCR_MXDMA_n(n)  ((n)<<RTL8139_RCR_MXDMA_BIT)

//
// RTL8139_IO_TCR: Tx Configuration Register (32-bit)

#define RTL8139_TCR_HWVERID_A_BIT   26
#define RTL8139_TCR_IFG_BIT         24
#define RTL8139_TCR_HWVERID_B_BIT   22
#define RTL8139_TCR_LBK_BIT         17
#define RTL8139_TCR_CRC_BIT         16
#define RTL8139_TCR_MXDMA_BIT       8
#define RTL8139_TCR_TXRR_BIT        4
#define RTL8139_TCR_CLRABT_BIT      0

#define RTL8139_TCR_HWVERID_A_BITS  5
#define RTL8139_TCR_IFG_BITS        2
#define RTL8139_TCR_HWVERID_B_BITS  2
#define RTL8139_TCR_LBK_BITS        2
#define RTL8139_TCR_MXDMA_BITS      3
#define RTL8139_TCR_TXRR_BITS       4

#define RTL8139_TCR_HWVERID_A_MASK  ((1U<<RTL8139_TCR_HWVERID_A_BITS)-1)
#define RTL8139_TCR_IFG_MASK        ((1U<<RTL8139_TCR_IFG_BITS)-1)
#define RTL8139_TCR_HWVERID_B_MASK  ((1U<<RTL8139_TCR_HWVERID_B_BITS)-1)
#define RTL8139_TCR_LBK_MASK        ((1U<<RTL8139_TCR_LBK_BITS)-1)
#define RTL8139_TCR_MXDMA_MASK      ((1U<<RTL8139_TCR_MXDMA_BITS)-1)
#define RTL8139_TCR_TXRR_MASK       ((1U<<RTL8139_TCR_TXRR_BITS)-1)

#define RTL8139_TCR_HWVERID_A \
    (RTL8139_TCR_HWVERID_A_MASK<<RTL8139_TCR_HWVERID_A_BIT)

#define RTL8139_TCR_IFG \
    (RTL8139_TCR_IFG_MASK<<RTL8139_TCR_IFG_BIT)

#define RTL8139_TCR_HWVERID_B \
    (RTL8139_TCR_HWVERID_B_MASK<<RTL8139_TCR_HWVERID_B_BIT)

#define RTL8139_TCR_LBK \
    (RTL8139_TCR_LBK_MASK<<RTL8139_TCR_LBK_BIT)

#define RTL8139_TCR_MXDMA \
    (RTL8139_TCR_MXDMA_MASK<<RTL8139_TCR_MXDMA_BIT)
#define RTL8139_TCR_TXRR \
    (RTL8139_TCR_TXRR_MASK<<RTL8139_TCR_TXRR_BIT)

// Append CRC
#define RTL8139_TCR_CRC             (1U<<RTL8139_TCR_CRC_BIT)

// Clear abort (only write 1 if in tx abort state)
#define RTL8139_TCR_CLRABT          (1U<<RTL8139_TCR_CLRABT_BIT)

// Hardware version ID A
#define RTL8139_TCR_HWVERID_A_n(n)  ((n)<<RTL8139_TCR_HWVERID_A_BIT)

// Interframe gap
#define RTL8139_TCR_IFG_n(n)        ((n)<<RTL8139_TCR_IFG_BIT)

// Hardware version ID B
#define RTL8139_TCR_HWVERID_B_n(n)  ((n)<<RTL8139_TCR_HWVERID_B_BIT)

// Loopback test
#define RTL8139_TCR_LBK_n(n)        ((n)<<RTL8139_TCR_LBK_BIT)

// Max tx DMA burst (1U<<(n+4))
#define RTL8139_TCR_MXDMA_n(n)      ((n)<<RTL8139_TCR_MXDMA_BIT)

// Tx retry count (16+n*16)
#define RTL8139_TCR_TXRR_n(n)       ((n)<<RTL8139_TCR_TXRR_BIT)

// RTL8139_IO_TSAD: Tx Status of All Descriptors (16-bit)

#define RTL8139_TSAD_OWN_BIT        0
#define RTL8139_TSAD_TABT_BIT       4
#define RTL8139_TSAD_TUN_BIT        8
#define RTL8139_TSAD_TOK_BIT        12

#define RTL8139_TSAD_OWN_n(n)       (1U<<((n)+RTL8139_TSAD_OWN_BIT))
#define RTL8139_TSAD_TABT_n(n)      (1U<<((n)+RTL8139_TSAD_TABT_BIT))
#define RTL8139_TSAD_TUN_n(n)       (1U<<((n)+RTL8139_TSAD_TUN_BIT))
#define RTL8139_TSAD_TOK_n(n)       (1U<<((n)+RTL8139_TSAD_TOK_BIT))

// Rx packet header
struct rtl8139_rx_hdr_t {
    uint16_t status;
    uint16_t len;
};

#define RTL8139_RX_HDR_MAR_BIT      15
#define RTL8139_RX_HDR_PAM_BIT      14
#define RTL8139_RX_HDR_BAR_BIT      13
#define RTL8139_RX_HDR_ISE_BIT      5
#define RTL8139_RX_HDR_RUNT_BIT     4
#define RTL8139_RX_HDR_LONG_BIT     3
#define RTL8139_RX_HDR_CRC_BIT      2
#define RTL8139_RX_HDR_FAE_BIT      1
#define RTL8139_RX_HDR_ROK_BIT      0

// Multicast address received
#define RTL8139_RX_HDR_MAR          (1U<<RTL8139_RX_HDR_MAR_BIT)

// Physical address matched
#define RTL8139_RX_HDR_PAM          (1U<<RTL8139_RX_HDR_PAM_BIT)

// Broadcast address received
#define RTL8139_RX_HDR_BAR          (1U<<RTL8139_RX_HDR_BAR_BIT)

// Invalid symbol error
#define RTL8139_RX_HDR_ISE          (1U<<RTL8139_RX_HDR_ISE_BIT)

// Runt packet received
#define RTL8139_RX_HDR_RUNT         (1U<<RTL8139_RX_HDR_RUNT_BIT)

// Long packet
#define RTL8139_RX_HDR_LONG         (1U<<RTL8139_RX_HDR_LONG_BIT)

// CRC error
#define RTL8139_RX_HDR_CRC          (1U<<RTL8139_RX_HDR_CRC_BIT)

// Frame alignment error
#define RTL8139_RX_HDR_FAE          (1U<<RTL8139_RX_HDR_FAE_BIT)

// Receive OK
#define RTL8139_RX_HDR_ROK          (1U<<RTL8139_RX_HDR_ROK_BIT)

void rtl8139_dev_t::tx_packet(int slot, ethq_pkt_t *pkt)
{
    assert(slot >= 0 && slot < 4);
    assert(pkt->size <= RTL8139_TSD_SIZE_MAX);

    if (pkt->size < 64) {
        memset((char*)&pkt->pkt + pkt->size, 0, 64 - pkt->size);
        pkt->size = 64;
    }

    assert(!tx_pkts[slot]);
    tx_pkts[slot] = pkt;

    RTL8139_TRACE("Transmitting packet addr=%p, len=%d slot=%d\n",
             (void*)pkt->physaddr, pkt->size, slot);

    RTL8139_MM_WR_32(RTL8139_IO_TSAD_n(slot), pkt->physaddr);

    RTL8139_MM_WR_32(RTL8139_IO_TSD_n(slot),
                     RTL8139_TSD_SIZE_n(pkt->size));
}

//
// IRQ handler

void rtl8139_dev_t::rx_irq_handler(uint16_t isr)
{
    RTL8139_TRACE("IRQ: Rx OK\n");

    if (isr & RTL8139_IxR_RXOVW) {
        RTL8139_TRACE("IRQ Rx: RXOVW\n");
        //rx_offset = 0;
        //RTL8139_MM_WR_16(RTL8139_IO_CAPR,
        //                 (uint16_t)rx_offset);
    }

    rtl8139_rx_hdr_t hdr;
    rtl8139_rx_hdr_t *hdr_ptr = (rtl8139_rx_hdr_t*)
            ((char*)rx_buffer + rx_offset);

    while (!(RTL8139_MM_RD_8(RTL8139_IO_CR) & RTL8139_CR_RXEMPTY)) {
        memcpy(&hdr, hdr_ptr, sizeof(hdr));

        RTL8139_TRACE("Header flags: %x len=%d\n",
                 hdr.status, hdr.len);

        ethq_pkt_t *pkt = ethq_pkt_acquire();
        if (pkt) {
            RTL8139_TRACE("Received packet, len=%d\n",
                     hdr.len);

            char const *pkt_st = (char const*)(hdr_ptr + 1);

            pkt->size = hdr.len - 4;

            if (rx_offset + hdr.len + 4 < 65536) {
                memcpy(&pkt->pkt, pkt_st, hdr.len);
            } else {
                size_t partial = 65536 - rx_offset - 4;
                memcpy(&pkt->pkt, pkt_st, partial);
                memcpy((char*)&pkt->pkt + partial, rx_buffer,
                       hdr.len - partial);
            }

            uint16_t ether_type = ntohs(pkt->pkt.hdr.len_ethertype);

            RTL8139_TRACE("Received packet from"
                          " %02x:%02x:%02x:%02x:%02x:%02x"
                          " %s=%04x\n",
                          pkt->pkt.hdr.s_mac[0],
                    pkt->pkt.hdr.s_mac[1],
                    pkt->pkt.hdr.s_mac[2],
                    pkt->pkt.hdr.s_mac[3],
                    pkt->pkt.hdr.s_mac[4],
                    pkt->pkt.hdr.s_mac[5],
                    ether_type >= 1536
                        ? "ethertype="
                        : ether_type <= 1500
                        ? "size="
                        : "?""?""?=",
                    ether_type);

            pkt->nic = (eth_dev_base_t *)this;

            ethq_enqueue(&rx_queue, pkt);
        }

        // Advance to next packet, skipping header, CRC,
        // and aligning to 32-bit boundary
        rx_offset = (rx_offset + hdr.len +
                           sizeof(rtl8139_rx_hdr_t) + 3) & -4;

        // Update read pointer
        if (rx_offset >= 65536)
            rx_offset -= 65536;

        hdr_ptr = (rtl8139_rx_hdr_t *)
                ((char*)rx_buffer + rx_offset);

        RTL8139_MM_WR_16(RTL8139_IO_CAPR, rx_offset - 16);
    }
}

void rtl8139_dev_t::tx_irq_handler()
{
    RTL8139_TRACE("IRQ: Tx OK\n");

    // Transmit status of all descriptors
    uint16_t tsad = RTL8139_MM_RD_16(RTL8139_IO_TSAD);

    RTL8139_TRACE("RX IRQ TSAD=%x\n", tsad);

    for (int i = 0; i < 4; ++i,
         (tx_tail = ((tx_tail + 1) & 3))) {
        ethq_pkt_t *pkt = tx_pkts[tx_tail];

        if (!pkt)
            break;

        if (tsad & RTL8139_TSAD_TOK_n(tx_tail)) {
            tx_pkts[tx_tail] = 0;

            RTL8139_TRACE("IRQ: TOK slot=%d\n", tx_tail);

            // Ignore slots that are not transmitting a packet
            if (!pkt) {
                RTL8139_TRACE("*** Spurious Tx OK IRQ"
                         " for slot=%d\n", tx_tail);
                continue;
            }

            // Packet successfully transmitted
            if (pkt->callback)
                pkt->callback(pkt, 0, pkt->callback_arg);

            // Return packet to pool
            ethq_pkt_release(pkt);

            // Get another outgoing packet
            pkt = ethq_dequeue(&tx_queue);

            //RTL8139_MM_WR_32(RTL8139_IO_TSD_n(tx_tail),
            //                 0);

            // Transmit new packet
            if (pkt) {
                assert(!tx_next[i]);
                tx_next[i] = pkt;
            }
        }

        if (tsad & RTL8139_TSAD_TABT_n(tx_tail)) {
            // Transmit aborted?

            // Ignore slots that are not transmitting a packet
            if (!pkt) {
                RTL8139_TRACE("*** Spurious transmit aborted IRQ"
                         " for slot=%d\n", tx_tail);
                continue;
            }

            RTL8139_TRACE("*** Transmit aborted?!\n");

            // Clear transmit abort
            RTL8139_MM_WR_32(RTL8139_IO_TCR,
                             RTL8139_MM_RD_32(RTL8139_IO_TCR) |
                             RTL8139_TCR_CLRABT);

            if (pkt->callback)
                pkt->callback(pkt, 1, pkt->callback_arg);
        }

        if (tsad & RTL8139_TSAD_TUN_n(tx_tail)) {
            // Transmit underrun?

            // Ignore slots that are not transmitting a packet
            if (!pkt) {
                RTL8139_TRACE("*** Spurious transmit aborted IRQ"
                         " for slot=%d\n", tx_tail);
                continue;
            }

            RTL8139_TRACE("*** transmit underrun?!\n");

            if (pkt->callback)
                pkt->callback(pkt, 1, pkt->callback_arg);
        }
    }
}

isr_context_t *rtl8139_dev_t::irq_dispatcher(int irq, isr_context_t *ctx)
{
    for (size_t i = 0; i < rtl8139_device_count; ++i) {
        rtl8139_dev_t *self = rtl8139_devices[i];

        int irq_offset = irq - self->irq_range.base;
        if (unlikely(irq_offset < 0 || irq_offset >= self->irq_range.count))
            continue;

        self->irq_handler();
    }

    return ctx;
}

void rtl8139_dev_t::irq_handler()
{
    scoped_lock lock_(lock);

    uint16_t isr = RTL8139_MM_RD_16(RTL8139_IO_ISR);

    // Acknowledge everything
    RTL8139_MM_WR_16(RTL8139_IO_ISR, isr);

    ethq_pkt_t *rx_first = 0;

    if (isr != 0) {
        RTL8139_TRACE("IRQ status = %x\n", isr);

        if (isr & RTL8139_IxR_SERR) {
            RTL8139_TRACE("*** IRQ: System Error\n");
        }

        if (isr & RTL8139_IxR_TOK) {
            tx_irq_handler();
        }

        if (isr & RTL8139_IxR_ROK) {
            rx_irq_handler(isr);
        }

        if (isr & RTL8139_IxR_FOVW) {
            RTL8139_TRACE("*** IRQ: Rx FIFO overflow\n");
        }

        if (isr & RTL8139_IxR_TER) {
            RTL8139_TRACE("*** IRQ: Tx Error\n");
        }

        if (isr & RTL8139_IxR_RER) {
            RTL8139_TRACE("*** IRQ: Rx Error\n");
        }

        if (isr & RTL8139_IxR_RXOVW) {
            RTL8139_TRACE("*** IRQ: Rx Overflow Error\n");
        }

        // Dequeue all received packets inside the lock
        rx_first = ethq_dequeue_all(&rx_queue);

        for (int n = 0; n < 4; ++n) {
            if (!tx_next[n])
                continue;

            tx_pkts[tx_head] = tx_next[n];
            tx_packet(tx_head, tx_next[n]);
            tx_next[n] = 0;
            tx_head = ((tx_head + 1) & 3);
        }
    }

    lock_.unlock();

    // Service the receive queue outside the lock
    if (rx_first) {
        ethq_pkt_t *rx_next;
        for (ethq_pkt_t *rx_packet = rx_first; rx_packet;
             rx_packet = rx_next) {
            rx_next = rx_packet->next;
            eth_frame_received(rx_packet);
            ethq_pkt_release(rx_packet);
        }
    }
}

int rtl8139_dev_t::send(ethq_pkt_t *pkt)
{
    scoped_lock lock_(lock);

    memcpy(pkt->pkt.hdr.s_mac, mac_addr, 6);

    if (!tx_pkts[tx_head]) {
        tx_packet(tx_head, pkt);
        tx_head = (tx_head + 1) & 3;
    } else {
        ethq_enqueue(&tx_queue, pkt);
    }

    return 1;
}

//
// Initialization

int rtl8139_factory_t::detect(eth_dev_base_t ***devices)
{
    pci_dev_iterator_t pci_iter;

    if (!pci_enumerate_begin(&pci_iter,
                             PCI_DEV_CLASS_NETWORK,
                             PCI_SUBCLASS_NETWORK_ETHERNET))
        return 0;

    do {
        if (pci_iter.config.vendor != 0x10EC ||
                pci_iter.config.device != 0x8139)
            continue;

        // Make sure we have an ethernet packet pool
        if (!ethq_init())
            panic("Out of memory!\n");

        printdbg("Detected PCI device %d/%d/%d"
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

        rtl8139_dev_t *self = (rtl8139_dev_t*)calloc(1, sizeof(rtl8139_dev_t));

        rtl8139_devices = (rtl8139_dev_t**)realloc(rtl8139_devices,
                                  sizeof(*rtl8139_devices) *
                                  (rtl8139_device_count + 1));
        rtl8139_devices[rtl8139_device_count++] = self;

        self->detect(pci_iter);
    } while (pci_enumerate_next(&pci_iter));

    *devices = (eth_dev_base_t**)rtl8139_devices;
    return rtl8139_device_count;
}

void rtl8139_dev_t::detect(pci_dev_iterator_t const &pci_dev)
{
    mmio_physaddr = pci_dev.config.base_addr[1] & -16;

    mmio = mmap((void*)mmio_physaddr, 256,
                      PROT_READ | PROT_WRITE,
                      MAP_PHYSICAL, -1, 0);

    // Enable MMIO and bus master, disable port I/O
    pci_adj_control_bits(pci_dev, PCI_CMD_BME | PCI_CMD_MSE,
                         PCI_CMD_IOSE);

    // Power on
    RTL8139_MM_WR_8(RTL8139_IO_CONFIG1, 0);

    // Reset
    RTL8139_MM_WR_8(RTL8139_IO_CR, RTL8139_CR_RST);

    // Allocate contiguous rx buffer
    rx_buffer_physaddr = mm_alloc_contiguous(65536 + 16);

    // Map rx buffer physical memory
    rx_buffer = mmap((void*)rx_buffer_physaddr, 65536 + 16,
                           PROT_READ, MAP_PHYSICAL,
                           -1, 0);

    // Wait for reset to finish
    while ((RTL8139_MM_RD_8(RTL8139_IO_CR) &
            RTL8139_CR_RST) != 0)
        pause();

    // Read MAC address
    uint32_t mac_hi;
    uint32_t mac_lo;
    mac_hi = RTL8139_MM_RD_32(RTL8139_IO_IDR_HI);
    mac_lo = RTL8139_MM_RD_32(RTL8139_IO_IDR_LO);

    memcpy(mac_addr + 4, &mac_lo, sizeof(uint16_t));
    memcpy(mac_addr, &mac_hi, sizeof(uint32_t));

    // Use MSI IRQ if possible
    use_msi = pci_try_msi_irq(pci_dev, &irq_range, 1, false, 1,
                              &rtl8139_dev_t::irq_dispatcher,
                              "rtl8139");

    RTL8139_TRACE("Using IRQs msi=%d, base=%u, count=%u\n",
               use_msi, irq_range.base, irq_range.count);

    // Reset receive ring buffer offset
    rx_offset = 0;
    RTL8139_MM_WR_16(RTL8139_IO_CAPR, 0);

    // Set rx buffer physical address
    RTL8139_MM_WR_32(RTL8139_IO_RBSTART,
                     (uint32_t)(uintptr_t)rx_buffer_physaddr);

    // Set tx buffer physical addresses
    for (unsigned i = 0; i < 4; ++i)
        RTL8139_MM_WR_32(RTL8139_IO_TSAD_n(i), 0);

    // Enable rx and tx
    RTL8139_MM_WR_8(RTL8139_IO_CR,
                    RTL8139_CR_RXEN |
                    RTL8139_CR_TXEN);

    RTL8139_MM_WR_32(RTL8139_IO_RCR,
                     // Early rx threshold 8=50%
                     RTL8139_RCR_ERTH_n(8) |
                     // Rx FIFO threshold 5=512 bytes
                     RTL8139_RCR_RXFTH_n(5) |
                     // Receive buffer length 3=64KB+16
                     RTL8139_RCR_RBLEN_n(3) |
                     // Max rx DMA 7=unlimited
                     RTL8139_RCR_MXDMA_n(7) |
                     // Accept broadcast
                     RTL8139_RCR_AB |
                     // Accept multicast
                     RTL8139_RCR_AM |
                     // Accept physical match
                     RTL8139_RCR_APM);

    // Tx configuration
    RTL8139_MM_WR_32(RTL8139_IO_TCR,
                     RTL8139_TCR_CRC |
                     RTL8139_TCR_IFG_n(3) |
                     RTL8139_TCR_TXRR_n(0) |
                     RTL8139_TCR_MXDMA_n(7) |
                     RTL8139_TCR_LBK_n(0));

    uint16_t unmask =
            RTL8139_IxR_SERR |
            RTL8139_IxR_TIMEOUT |
            RTL8139_IxR_FOVW |
            RTL8139_IxR_PUNLC |
            RTL8139_IxR_RXOVW |
            RTL8139_IxR_TER |
            RTL8139_IxR_TOK |
            RTL8139_IxR_RER |
            RTL8139_IxR_ROK;

    // Acknowledge IRQs
    //RTL8139_MM_WR_16(RTL8139_IO_ISR, unmask);

    // Unmask IRQs
    RTL8139_MM_WR_16(RTL8139_IO_IMR, unmask);
}

void rtl8139_dev_t::get_mac(void *mac_addr_ret)
{
    memcpy(mac_addr_ret, mac_addr, 6);
}

void rtl8139_dev_t::set_mac(void const *mac_addr_set)
{
    memcpy(mac_addr, mac_addr_set, 6);

    uint32_t mac_hi;
    uint32_t mac_lo;

    memcpy(&mac_lo, mac_addr + 4, sizeof(uint16_t));
    memcpy(&mac_hi, mac_addr, sizeof(uint32_t));

    RTL8139_MM_WR_32(RTL8139_IO_IDR_HI, mac_hi);
    RTL8139_MM_WR_32(RTL8139_IO_IDR_LO, mac_lo);
}

int rtl8139_dev_t::get_promiscuous()
{
    return !!(RTL8139_MM_RD_32(RTL8139_IO_RCR) &
            RTL8139_RCR_AAP);
}

void rtl8139_dev_t::set_promiscuous(int promiscuous)
{
    RTL8139_MM_WR_32(RTL8139_IO_RCR,
                     (RTL8139_MM_RD_32(RTL8139_IO_RCR) &
                     ~RTL8139_RCR_AAP) |
                     (!!promiscuous) << RTL8139_RCR_AAP_BIT);
}

#if 0
static void rtl8139_startup_hack(void *p)
{
    (void)p;
    rtl8139_detect();

    if (rtl8139_device_count == 0)
        return;

    ipv4_addr_pair_t bind = {
        { IPV4_ADDR32(255,255,255,255), 67, 0 },
        { IPV4_ADDR32(255,255,255,255), 68, 0 }
    };

    int handle = udp_bind(&bind);

    (void)handle;

    rtl8139_dev_t *self = rtl8139_devices[0];

    // Send a DHCP discover
    for (size_t i = 0; i < 10; ++i) {
        ethq_pkt_t *pkt = ethq_pkt_acquire();
        dhcp_pkt_t *discover = (void*)&pkt->pkt;

        pkt->size = dhcp_build_discover(discover, self->mac_addr);

        RTL8139_TRACE("Sending pkt 0x%p\n", (void*)pkt);

        sleep(100);
        rtl8139_send(self, pkt);
    }
}
REGISTER_CALLOUT(rtl8139_startup_hack, 0, 'L', "000");
#endif
