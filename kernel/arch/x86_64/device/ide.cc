#include "dev_storage.h"
#include "cpu/ioport.h"
#include "pci.h"
#include "mm.h"
#include "string.h"
#include "time.h"
#include "printk.h"
#include "cpu/atomic.h"
#include "cpu/control_regs.h"
#include "threadsync.h"
#include "bswap.h"

#define IDE_DEBUG   1
#if IDE_DEBUG
#define IDE_TRACE(...) printdbg("ide: " __VA_ARGS__)
#else
#define IDE_TRACE(...) ((void)0)
#endif

struct ide_chan_ports_t {
    ioport_t cmd;
    ioport_t ctl;
    uint8_t irq;
};

struct ide_if_factory_t : public storage_if_factory_t {
    ide_if_factory_t() : storage_if_factory_t("ide") {}
    virtual if_list_t detect(void);
};

static ide_if_factory_t ide_factory;

struct ide_if_t : public storage_if_base_t {
    STORAGE_IF_IMPL

    struct bmdma_prd_t {
        uint32_t physaddr;
        uint16_t size;
        uint16_t eot;
    } __attribute__((packed));

    C_ASSERT(sizeof(bmdma_prd_t) == 8);

    struct ide_chan_t {
        ide_if_t *if_;
        int secondary;

        uint32_t max_multiple;
        uint16_t has_48bit;
        uint8_t read_command;
        uint8_t write_command;
        int8_t max_dma;
        int8_t old_dma;

        ide_chan_ports_t ports;
        int busy;
        int io_done;
        int io_status;
        mutex_t lock;
        condition_var_t not_busy_cond;
        condition_var_t done_cond;

        void *io_window;
        void *dma_bounce;
        bmdma_prd_t *dma_prd;
        uintptr_t dma_prd_physaddr;
        mmphysrange_t io_window_ranges[PAGESIZE / sizeof(uint64_t)];

        ide_chan_t();

        int acquire_access();
        void release_access(int intr_was_enabled);
        void detect_devices();
        void set_drv(int slave);
        void set_lba(uint64_t lba);
        void set_count(int count);
        void set_feature(uint8_t feature, uint8_t p1 = 0, uint8_t p2 = 0,
                         uint8_t p3 = 0, uint8_t p4 = 0);
        uint8_t wait_not_bsy();
        uint8_t wait_drq();
        void issue_command(uint8_t command);
        void issue_packet_read(uint64_t lba, uint32_t count,
                               uint16_t burst_len, int use_dma);
        void set_irq_enable(int enable);
        void hex_dump(void const *mem, size_t size);

        int64_t io(void *data, int64_t count, uint64_t lba,
                   int slave, int is_atapi, int is_read);

        static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
        void irq_handler();

        int flush(int slave, int is_atapi);

        void yield_until_irq();

        int sector_size() const;
    };

    ide_if_t();

    uint8_t bmdma_inb(uint16_t reg);
    void bmdma_outb(uint16_t reg, uint8_t value);
    void bmdma_outd(uint16_t reg, uint32_t value);

    // Wait for and release dma hardware (no-op if bmdma is not simplex)
    void bmdma_acquire();
    void bmdma_release();

    ide_chan_t chan[2];
    ioport_t bmdma_base;

    // Handle simplex DMA controllers
    // (which can only work for one channel at one time)
    int simplex_dma;
    int dma_inuse;
    mutex_t dma_lock;
    condition_var_t dma_available;
};

struct ide_dev_t : public storage_dev_base_t {
    STORAGE_DEV_IMPL

    ide_if_t::ide_chan_t *chan;
    int slave;
    int is_atapi;
};

#define IDE_MAX_IF 8
static ide_if_t ide_ifs[IDE_MAX_IF];
static size_t ide_if_count;

#define IDE_MAX_DEVS    16
static ide_dev_t ide_devs[IDE_MAX_DEVS];
static size_t ide_dev_count;

//
// Commands

#define ATA_CMD_READ_PIO            0x20
#define ATA_CMD_READ_PIO_EXT        0x24
#define ATA_CMD_READ_MULT_PIO       0xC5
#define ATA_CMD_READ_MULT_PIO_EXT   0x29
#define ATA_CMD_READ_DMA            0xC8
#define ATA_CMD_READ_DMA_EXT        0x25

#define ATA_CMD_WRITE_PIO           0x30
#define ATA_CMD_WRITE_PIO_EXT       0x34
#define ATA_CMD_WRITE_MULT_PIO      0xC4
#define ATA_CMD_WRITE_MULT_PIO_EXT  0x39
#define ATA_CMD_WRITE_DMA           0xCA
#define ATA_CMD_WRITE_DMA_EXT       0x35

#define ATA_CMD_SET_MULTIPLE        0xC6

#define ATA_CMD_CACHE_FLUSH         0xE7
#define ATA_CMD_CACHE_FLUSH_EXT     0xEA

#define ATA_CMD_PACKET              0xA0

#define ATA_CMD_IDENTIFY_PACKET     0xA1
#define ATA_CMD_IDENTIFY            0xEC

#define ATA_CMD_SET_FEATURE         0xEF

//
// ATAPI commands

#define      ATAPI_CMD_READ         0xA8
#define      ATAPI_CMD_EJECT        0x1B

//
// Task file

#if 1
#define ATA_REG_CMD_DATA            0x00
#define ATA_REG_CMD_FEATURES        0x01    // write only
#define ATA_REG_CMD_ERROR           0x01    // read only
#define ATA_REG_CMD_SECCOUNT0       0x02
#define ATA_REG_CMD_LBA0            0x03
#define ATA_REG_CMD_LBA1            0x04
#define ATA_REG_CMD_LBA2            0x05
#define ATA_REG_CMD_HDDEVSEL        0x06
#define ATA_REG_CMD_COMMAND         0x07    // write only
#define ATA_REG_CMD_STATUS          0x07    // read only

// ctl ports
#define ATA_REG_CTL_ALTSTATUS       0x00    // read only
#define ATA_REG_CTL_CONTROL         0x00
#else
// cmd ports (wtf?)
#define ATA_REG_CMD_DATA            0x00
#define ATA_REG_CMD_FEATURES        0x01    // write only
#define ATA_REG_CMD_ERROR           0x01    // read only
#define ATA_REG_CMD_SECCOUNT0       0x02
#define ATA_REG_CMD_LBA0            0x03
#define ATA_REG_CMD_LBA1            0x04
#define ATA_REG_CMD_LBA2            0x05
#define ATA_REG_CMD_HDDEVSEL        0x06
#define ATA_REG_CMD_COMMAND         0x07    // write only
#define ATA_REG_CMD_STATUS          0x07    // read only
#define ATA_REG_CMD_SECCOUNT1       0x08
#define ATA_REG_CMD_LBA3            0x09
#define ATA_REG_CMD_LBA4            0x0A
#define ATA_REG_CMD_LBA5            0x0B
#define ATA_REG_CMD_CONTROL         0x0C    // write only

// ctl ports
#define ATA_REG_CTL_ALTSTATUS       0x02    // read only
#define ATA_REG_CTL_DRVADDRESS      0x03
#endif

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
// Identify response

#define ATA_IDENTIFY_MAX_MULTIPLE   47
#define ATA_IDENTIFY_OLD_DMA        63
#define ATA_IDENTIFY_CMD_SETS_2     83
#define ATA_IDENTIFY_VALIDITY       53
#define ATA_IDENTIFY_UDMA_SUPPORT   88

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

// BMDMA
//
// +-----+---------+--------------------------------------------+----+
// |     |    Addr | Description                                | Ch |
// +-----+---------+--------------------------------------------+----+
// | R/W |     00h | Bus Master IDE Command register Primary    |  1 |
// | RWC |     02h | Bus Master IDE Status register Primary     |  1 |
// | R/W | 04h-07h | Bus Master IDE PRD Table Address Primary   |  1 |
// | R/W |     08h | Bus Master IDE Command register Secondary  |  2 |
// | RWC |     0Ah | Bus Master IDE Status register Secondary   |  2 |
// | R/W | 0Ch-0Fh | Bus Master IDE PRD Table Address Secondary |  2 |
// +-----+---------+--------------------------------------------+----+
// |     |     01h | Device Specific                            |    |
// |     |     03h | Device Specific                            |    |
// |     |     09h | Device Specific                            |    |
// |     |     0Bh | Device Specific                            |    |
// +-----+---------+--------------------------------------------+----+

#define ATA_BMDMA_REG_CMD_n(secondary)      ((((secondary) != 0) << 3) + 0)
#define ATA_BMDMA_REG_STATUS_n(secondary)   ((((secondary) != 0) << 3) + 2)
#define ATA_BMDMA_REG_PRD_n(secondary)      ((((secondary) != 0) << 3) + 4)

if_list_t ide_if_factory_t::detect(void)
{
    unsigned start_at = ide_if_count;

    if_list_t list = {
        ide_ifs + start_at,
        sizeof(*ide_ifs),
        0
    };

    pci_dev_iterator_t pci_iter;

    IDE_TRACE("Enumerating PCI busses for IDE\n");

    if (!pci_enumerate_begin(
                &pci_iter,
                PCI_DEV_CLASS_STORAGE,
                PCI_SUBCLASS_STORAGE_IDE))
        return list;

    static ioport_t std_ports[] = {
        0x1F0, 0x3F6,
        0x170, 0x376,
        0x1E8, 0x3EE,
        0x168, 0x36E
    };

    size_t std_idx = 0;

    do {
        ide_if_t *dev = ide_ifs + ide_if_count++;

        dev->chan[0].ports.cmd = (pci_iter.config.base_addr[0] > 1)
                ? pci_iter.config.base_addr[0]
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        dev->chan[0].ports.ctl = (pci_iter.config.base_addr[1] > 1)
                ? pci_iter.config.base_addr[1]
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        dev->chan[1].ports.cmd = (pci_iter.config.base_addr[2] > 1)
                ? pci_iter.config.base_addr[2]
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        dev->chan[1].ports.ctl = (pci_iter.config.base_addr[3] > 1)
                ? pci_iter.config.base_addr[3]
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        dev->bmdma_base = pci_iter.config.base_addr[4];

        if (pci_iter.config.prog_if == 0x8A ||
                pci_iter.config.prog_if == 0x80) {
            IDE_TRACE("On IRQ 14 and 15\n");
            dev->chan[0].ports.irq = 14;
            dev->chan[1].ports.irq = 15;
        } else if (pci_iter.config.irq_line != 0xFE) {
            IDE_TRACE("Both on IRQ %d\n", pci_iter.config.irq_line);
            dev->chan[0].ports.irq = pci_iter.config.irq_line;
            dev->chan[1].ports.irq = pci_iter.config.irq_line;
        } else {
            IDE_TRACE("Both on IRQ 14\n");
            dev->chan[0].ports.irq = 14;
            dev->chan[1].ports.irq = 14;
            IDE_TRACE("Updating PCI config IRQ line\n");
            pci_config_write(pci_iter.bus, pci_iter.slot, pci_iter.func,
                           offsetof(pci_config_hdr_t, irq_line),
                           &dev->chan[0].ports.irq, sizeof(uint8_t));
        }

        IDE_TRACE("Found PCI IDE interface at 0x%x/0x%x/irq%d"
                  " - 0x%x/0x%x/irq%d\n",
                  dev->chan[0].ports.cmd,
                dev->chan[0].ports.ctl, dev->chan[0].ports.irq,
                dev->chan[1].ports.cmd, dev->chan[1].ports.ctl,
                dev->chan[1].ports.irq);

        // Sanity check ports, reject entry if any are zero
        if (dev->chan[0].ports.cmd == 0 ||
                dev->chan[0].ports.ctl == 0 ||
                dev->chan[1].ports.cmd == 0 ||
                dev->chan[1].ports.ctl == 0) {
            --ide_if_count;
            continue;
        }

        // Enable bus master DMA
        pci_adj_control_bits(pci_iter.bus, pci_iter.slot,
                             pci_iter.func, PCI_CMD_BUSMASTER, 0);

    } while (pci_enumerate_next(&pci_iter));

    list.count = ide_if_count - start_at;

    return list;
}

void ide_if_t::cleanup()
{
}

uint8_t ide_if_t::ide_chan_t::wait_not_bsy()
{
    uint8_t status;

    for (;; pause()) {
        status = inb(ports.cmd + ATA_REG_CMD_STATUS);

        if ((!(status & ATA_REG_STATUS_BSY)) || (status == 0xFF))
            break;
    }

    return status;
}

uint8_t ide_if_t::ide_chan_t::wait_drq()
{
    uint8_t status;

    for (;; pause()) {
        status = inb(ports.cmd + ATA_REG_CMD_STATUS);

        if ((status & ATA_REG_STATUS_DRQ) || (status == 0xFF))
            break;
    }

    return status;
}

void ide_if_t::ide_chan_t::issue_command(uint8_t command)
{
    outb(ports.cmd + ATA_REG_CMD_COMMAND, command);
}

void ide_if_t::ide_chan_t::issue_packet_read(
        uint64_t lba, uint32_t count, uint16_t burst_len, int use_dma)
{
    outb(ports.cmd + ATA_REG_CMD_FEATURES, use_dma ? 1 : 0);
    outb(ports.cmd + ATA_REG_CMD_SECCOUNT0, 0);
    outb(ports.cmd + ATA_REG_CMD_LBA0, 0);
    outb(ports.cmd + ATA_REG_CMD_LBA1, (burst_len >> (8*0)) & 0xFF);
    outb(ports.cmd + ATA_REG_CMD_LBA2, (burst_len >> (8*1)) & 0xFF);
    set_irq_enable(0);
    issue_command(ATA_CMD_PACKET);

    uint8_t packet[12] = {
        0xA8,
        0,
        uint8_t((lba >> 24) & 0xFF),
        uint8_t((lba >> 16) & 0xFF),
        uint8_t((lba >>  8) & 0xFF),
        uint8_t((lba >>  0) & 0xFF),
        uint8_t((count >> 24) & 0xFF),
        uint8_t((count >> 16) & 0xFF),
        uint8_t((count >>  8) & 0xFF),
        uint8_t((count >>  0) & 0xFF),
        0,
        0
    };

    wait_not_bsy();
    wait_drq();

    set_irq_enable(1);
    outsw(ports.cmd + ATA_REG_CMD_DATA,
          packet, sizeof(packet) / sizeof(uint16_t));
}

void ide_if_t::ide_chan_t::set_irq_enable(int enable)
{
    outb(ports.ctl + ATA_REG_CTL_CONTROL, ATA_REG_CONTROL_n(
             enable ? 0 : ATA_REG_CONTROL_nIEN));
}

void ide_if_t::ide_chan_t::hex_dump(void const *mem, size_t size)
{
    uint8_t *buf = (uint8_t*)mem;
    char line_buf[8 + 1 + 1 + 3*16 + 1 + 16 + 2];

    int line_ofs;
    for (size_t i = 0; i < size; ++i) {
        if (!(i & 15)) {
            line_ofs = snprintf(line_buf, sizeof(line_buf), "%08zx: ", i);
        }

        line_ofs += snprintf(line_buf + line_ofs,
                             sizeof(line_buf) - line_ofs,
                             "%02x ", buf[i]);

        if ((i & 15) == 15) {
            line_buf[line_ofs++] = ' ';

            for (int k = -15; k <= 0; ++k)
                line_buf[line_ofs++] =
                        buf[i + k] < ' ' ? '.' : buf[i + k];

            line_buf[line_ofs++] = '\n';
            line_buf[line_ofs++] = 0;
            IDE_TRACE("%s", line_buf);
        }
    }
}

void ide_if_t::ide_chan_t::set_drv(int slave)
{
    outb(ports.cmd + ATA_REG_CMD_HDDEVSEL,
         ATA_REG_HDDEVSEL_n((slave ? ATA_REG_HDDEVSEL_DRV : 0) |
         ATA_REG_HDDEVSEL_LBA));
}

void ide_if_t::ide_chan_t::set_lba(uint64_t lba)
{
    assert(lba < (uint64_t(1) << (6*8)));

    if (has_48bit) {
        outb(ports.cmd + ATA_REG_CMD_LBA0, (lba >> (3*8)) & 0xFF);
        outb(ports.cmd + ATA_REG_CMD_LBA1, (lba >> (4*8)) & 0xFF);
        outb(ports.cmd + ATA_REG_CMD_LBA2, (lba >> (5*8)) & 0xFF);
    }
    outb(ports.cmd + ATA_REG_CMD_LBA0, (lba >> (0*8)) & 0xFF);
    outb(ports.cmd + ATA_REG_CMD_LBA1, (lba >> (1*8)) & 0xFF);
    outb(ports.cmd + ATA_REG_CMD_LBA2, (lba >> (2*8)) & 0xFF);
}

void ide_if_t::ide_chan_t::set_count(int count)
{
    assert(count <= ((has_48bit && max_multiple > 1) ? 65536 : 256));
    if (has_48bit)
        outb(ports.cmd + ATA_REG_CMD_SECCOUNT0, (count >> (1*8)) & 0xFF);
    outb(ports.cmd + ATA_REG_CMD_SECCOUNT0, (count >> (0*8)) & 0xFF);
}

void ide_if_t::ide_chan_t::set_feature(
        uint8_t feature, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4)
{
    outb(ports.cmd + ATA_REG_CMD_FEATURES, feature);
    outb(ports.cmd + ATA_REG_CMD_SECCOUNT0, p1);
    outb(ports.cmd + ATA_REG_CMD_LBA0, p2);
    outb(ports.cmd + ATA_REG_CMD_LBA1, p3);
    outb(ports.cmd + ATA_REG_CMD_LBA2, p4);
    outb(ports.cmd + ATA_REG_CMD_COMMAND, ATA_CMD_SET_FEATURE);
}

ide_if_t::ide_chan_t::ide_chan_t()
{
    mutex_init(&lock);
    condvar_init(&not_busy_cond);
    condvar_init(&done_cond);
}

int ide_if_t::ide_chan_t::acquire_access()
{
    int intr_was_enabled = cpu_irq_disable();
    mutex_lock(&lock);

    while (busy)
        condvar_wait(&not_busy_cond, &lock);

    busy = 1;
    mutex_unlock(&lock);
    return intr_was_enabled;
}

void ide_if_t::ide_chan_t::release_access(int intr_was_enabled)
{
    mutex_lock(&lock);
    busy = 0;
    mutex_unlock(&lock);
    condvar_wake_one(&not_busy_cond);
    cpu_irq_toggle(intr_was_enabled);
}

void ide_if_t::ide_chan_t::detect_devices()
{
    IDE_TRACE("Detecting IDE devices\n");

    for (int slave = 0; slave < 2; ++slave) {
        // Wait for not busy
        uint8_t status = wait_not_bsy();

        // If status == 0, no device
        if (status == 0)
            continue;

        IDE_TRACE("if=%d, dev=%d, status=0x%x\n", secondary, slave, status);

        // Set LBA to zero for ATAPI detection later
        set_drv(slave);
        set_lba(0);
        set_irq_enable(0);

        IDE_TRACE("if=%d, dev=%d, issuing IDENTIFY\n", secondary, slave);
        issue_command(ATA_CMD_IDENTIFY);

        int is_atapi = 0;
        int is_err = 0;

        for (;; pause()) {
            status = inb(ports.cmd + ATA_REG_CMD_STATUS);

            // If no drive, give up
            if (status == 0)
                break;

            // Wait until not busy
            if (status & ATA_REG_STATUS_BSY)
                continue;

            // If error bit set, it failed
            if (!is_atapi && (status & ATA_REG_STATUS_ERR)) {
                uint8_t lba1 = inb(ports.cmd + ATA_REG_CMD_LBA1);
                uint8_t lba2 = inb(ports.cmd + ATA_REG_CMD_LBA2);

                // Check for ATAPI signature
                if (lba1 == 0x14 && lba2 == 0xEB)
                    is_atapi = 1;
                else if (lba1 == 0x69 && lba2 == 0x96)
                    is_atapi = 1;

                if (is_atapi) {
                    // Issue identify command
                    issue_command(ATA_CMD_IDENTIFY_PACKET);
                    continue;
                }

                break;
            }

            // If not busy and data ready, stop spinning
            if (status & ATA_REG_STATUS_DRDY)
                break;
        }

        int sector_size = !is_atapi ? 512 : 2048;

        // If status == 0, no device
        if (status == 0)
            continue;

        // Check error bit in status register
        is_err = (status & ATA_REG_STATUS_ERR) != 0;

        // If error bit set, read error register
        uint8_t err = 0;
        if (is_err)
            err = inb(ports.cmd + ATA_REG_CMD_ERROR);

        // If error and command aborted, there is no drive
        if (is_err && (err & ATA_REG_ERROR_ABRT)) {
            IDE_TRACE("if=%d, dev=%d, error=0x%x, no drive\n",
                      secondary, slave, err);
            continue;
        }

        // If error, then we do not know what happened
        if (is_err) {
            IDE_TRACE("if=%d, dev=%d, error=0x%x\n", secondary, slave, err);
            continue;
        }

        ide_devs[ide_dev_count].chan = this;
        ide_devs[ide_dev_count].slave = slave;
        ide_devs[ide_dev_count].is_atapi = is_atapi;
        ++ide_dev_count;

        uint16_t *buf = new uint16_t[256];

        IDE_TRACE("if=%d, dev=%d, receiving IDENTIFY\n", secondary, slave);

        // Read IDENTIFY data
        memset(buf, 0, 512);
        insw(ports.cmd + ATA_REG_CMD_DATA, buf, 512 / sizeof(uint16_t));
        hex_dump(buf, 512);

        max_multiple = buf[ATA_IDENTIFY_MAX_MULTIPLE] & 0xFF;

        if (max_multiple == 0)
            max_multiple = 1;

        has_48bit = (buf[ATA_IDENTIFY_CMD_SETS_2] >> 10) & 1;

        IDE_TRACE("if=%d, dev=%d, has_48bit=%d\n",
                  secondary, slave, has_48bit);

        // Force dma_support to 0 if interface does not support UDMA,
        // otherwise, force dma_support to 0 if validity bit does not report
        // support for UDMA,
        // otherwise, use max supported UDMA value
        uint16_t dma_support = (if_->bmdma_base
                ? (buf[ATA_IDENTIFY_VALIDITY] & (1 << 2))
                : 0)
                ? buf[ATA_IDENTIFY_UDMA_SUPPORT]
                : 0;

        // Look for highest supported UDMA
        for (max_dma = 5; max_dma >= 0; --max_dma) {
            if (dma_support & (1 << max_dma))
                break;
        }

        if (max_dma < 0) {
            // Look for old DMA
            dma_support = if_->bmdma_base
                    ? buf[ATA_IDENTIFY_OLD_DMA]
                    : 0;

            for (max_dma = 2; max_dma >= 0; --max_dma) {
                if (dma_support & (1 << max_dma))
                    break;
            }

            if (max_dma >= 0)
                old_dma = 1;
        }

        delete[] buf;

        IDE_TRACE("if=%d, dev=%d, max_dma=%d\n", secondary, slave, max_dma);

        if (has_48bit) {
            if (max_dma >= 0) {
                read_command = ATA_CMD_READ_DMA_EXT;
                write_command = ATA_CMD_WRITE_DMA_EXT;
                max_multiple = 65536;
            } else if (max_multiple > 1) {
                read_command = ATA_CMD_READ_MULT_PIO_EXT;
                write_command = ATA_CMD_WRITE_MULT_PIO_EXT;
            } else {
                read_command = ATA_CMD_READ_PIO_EXT;
                write_command = ATA_CMD_WRITE_PIO_EXT;
            }
        } else {
            if (max_dma >= 0) {
                read_command = ATA_CMD_READ_DMA;
                write_command = ATA_CMD_WRITE_DMA;
                max_multiple = 256;
            } else if (max_multiple > 1) {
                read_command = ATA_CMD_READ_MULT_PIO;
                write_command = ATA_CMD_WRITE_MULT_PIO;
            } else {
                read_command = ATA_CMD_READ_PIO;
                write_command = ATA_CMD_WRITE_PIO;
            }
        }

        if (max_dma >= 0) {
            wait_not_bsy();

            if (!old_dma) {
                // Enable UDMA
                IDE_TRACE("if=%d, dev=%d, enabling UDMA\n", secondary, slave);
                set_feature(ATA_FEATURE_XFER_MODE,
                            ATA_FEATURE_XFER_MODE_UDMA_n(max_dma));
            } else {
                // Enable MWDMA
                IDE_TRACE("if=%d, dev=%d, enabling MWDMA\n", secondary, slave);
                set_feature(ATA_FEATURE_XFER_MODE,
                            ATA_FEATURE_XFER_MODE_MWDMA_n(max_dma));
            }

            // Allocate page for PRD list
            dma_prd = (bmdma_prd_t*)mmap(0, PAGESIZE, PROT_READ | PROT_WRITE,
                           MAP_32BIT | MAP_POPULATE, -1, 0);

            // Get physical address of PRD list
            dma_prd_physaddr = mphysaddr(dma_prd);

            // Allocate bounce buffer for DMA
            dma_bounce = mmap(0, max_multiple * sector_size,
                              PROT_READ | PROT_WRITE,
                              MAP_32BIT | MAP_POPULATE, -1, 0);
            io_window = mmap_window(max_multiple * sector_size);

            // Get list of physical address ranges for io_window
            size_t range_count = mphysranges(
                        io_window_ranges, countof(io_window_ranges),
                        dma_bounce, max_multiple * sector_size, 32768);

            range_count = mphysranges_split(
                        io_window_ranges, range_count,
                        countof(io_window_ranges), 16);

            // Populate PRD list with bounce buffer addresses
            for (size_t i = 0; i < range_count; ++i) {
                dma_prd[i].physaddr = io_window_ranges[i].physaddr;
                dma_prd[i].size = io_window_ranges[i].size;
                dma_prd[i].eot = (i == (range_count-1)) ? 0x8000 : 0;
            }

            // Set PRD address register
            assert(dma_prd_physaddr && dma_prd_physaddr < 0x100000000);
            if_->bmdma_outd(ATA_BMDMA_REG_PRD_n(secondary),
                            (uint32_t)dma_prd_physaddr);
        } else if (max_multiple > 1) {
            IDE_TRACE("if=%d, dev=%d, enabling MULTIPLE\n", secondary, slave);

            wait_not_bsy();
            set_count(max_multiple);
            set_lba(0);
            issue_command(ATA_CMD_SET_MULTIPLE);
            status = wait_not_bsy();

            if (status & ATA_REG_STATUS_ERR) {
                IDE_TRACE("Set multiple command failed\n");
            }

            io_window = mmap_window(max_multiple * sector_size);
        } else {
            IDE_TRACE("if=%d, dev=%d, single sector (old)\n", secondary, slave);

            io_window = mmap_window(sector_size);
        }

        set_irq_enable(1);

        irq_hook(ports.irq, &ide_chan_t::irq_handler);
        irq_setmask(ports.irq, 1);

        IDE_TRACE("if=%d, dev=%d, done\n", secondary, slave);
    }
}

ide_if_t::ide_if_t()
{
    chan[0].if_ = this;
    chan[1].if_ = this;
    chan[1].secondary = 1;
}

uint8_t ide_if_t::bmdma_inb(uint16_t reg)
{
    return inb((bmdma_base & -4) + reg);
}

void ide_if_t::bmdma_outb(uint16_t reg, uint8_t value)
{
    outb((bmdma_base & -4) + reg, value);
}

void ide_if_t::bmdma_outd(uint16_t reg, uint32_t value)
{
    outd((bmdma_base & -4) + reg, value);
}

void ide_if_t::bmdma_acquire()
{
    if (simplex_dma) {
        mutex_lock(&dma_lock);
        while (dma_inuse)
            condvar_wait(&dma_available, &dma_lock);
        dma_inuse = 1;
        mutex_unlock(&dma_lock);
    }
}

void ide_if_t::bmdma_release()
{
    if (simplex_dma) {
        mutex_lock(&dma_lock);
        dma_inuse = 0;
        mutex_unlock(&dma_lock);
        condvar_wake_one(&dma_available);
    }
}

void ide_if_t::ide_chan_t::yield_until_irq()
{
    mutex_lock(&lock);
    while (!io_done)
        condvar_wait(&done_cond, &lock);
    mutex_unlock(&lock);
}

int64_t ide_if_t::ide_chan_t::io(
        void *data, int64_t count, uint64_t lba,
        int slave, int is_atapi, int is_read)
{
    int err = 0;

    if (unlikely(count == 0))
        return err;

    int sector_size = !is_atapi ? 512 : 2048;

    IDE_TRACE("Read lba=%lu, max_multiple=%u, count=%ld\n",
              lba, max_multiple, count);

    int64_t read_count = 0;

    int intr_was_enabled = acquire_access();

    wait_not_bsy();
    set_drv(slave);
    set_irq_enable(1);

    for (int64_t count_base = 0;
         err == 0 && count_base < count;
         count_base += max_multiple) {
        unsigned sub_count = count - count_base;

        if (sub_count > max_multiple)
            sub_count = max_multiple;

        IDE_TRACE("sub_count=%u\n", sub_count);

        size_t sub_size = sector_size * sub_count;

        mutex_lock(&lock);
        io_done = 0;
        mutex_unlock(&lock);

        if (max_dma >= 0) {
            if_->bmdma_acquire();

            // Set PRD address register
            assert(dma_prd_physaddr && dma_prd_physaddr < 0x100000000);
            if_->bmdma_outd(ATA_BMDMA_REG_PRD_n(secondary),
                            (uint32_t)dma_prd_physaddr);
        }

        if (!is_atapi) {
            set_lba(lba + count_base);
            set_count(sub_count);
            issue_command(is_read ? read_command : write_command);
        } else {
            issue_packet_read(lba + count_base, sub_count, 2048, max_dma >= 0);
        }

        size_t io_window_misalignment = (uintptr_t)data & ~(int)-PAGESIZE;

        if (max_dma >= 0) {
            IDE_TRACE("Starting DMA\n");

            // Program DMA reads or writes and start DMA
            if_->bmdma_outb(ATA_BMDMA_REG_CMD_n(secondary), is_read ? 9 : 1);

            // Get physical addresses of destination
            size_t range_count = mphysranges(
                        io_window_ranges, countof(io_window_ranges),
                        data, sub_size + io_window_misalignment, ~0UL);

            // Map window to modify the memory
            alias_window(io_window, sub_size, io_window_ranges, range_count);
        }

        yield_until_irq();

        if (io_status & ATA_REG_STATUS_ERR) {
            IDE_TRACE("error! status=0x%x, err=0x%x", io_status,
                      inb(ports.cmd + ATA_REG_CMD_ERROR));
            err = 1;
        }

        if (max_dma >= 0) {
            // Must read status register to synchronize with bus master
            IDE_TRACE("Reading DMA status\n");
            uint8_t bmdma_status = if_->bmdma_inb(
                        ATA_BMDMA_REG_STATUS_n(secondary));

            // Write 1 to interrupt bit to acknowledge
            if_->bmdma_outb(ATA_BMDMA_REG_STATUS_n(secondary),
                            bmdma_status | (1 << 2));
            IDE_TRACE("bmdma status=0x%x\n", bmdma_status);

            // Stop DMA
            IDE_TRACE("Stopping DMA\n");
            if_->bmdma_outb(ATA_BMDMA_REG_CMD_n(secondary), 0);

            if_->bmdma_release();

            // Copy into caller's data buffer
            memcpy((char*)io_window + io_window_misalignment,
                   dma_bounce, sub_size);
        } else {
            size_t adj_size = (sub_size) + io_window_misalignment;
            char *aligned_addr = (char*)data - io_window_misalignment;

            size_t ranges_count = mphysranges(
                        io_window_ranges, countof(io_window_ranges),
                        aligned_addr, adj_size, ~0U);

            alias_window(io_window, sub_size,
                         io_window_ranges, ranges_count);

            IDE_TRACE("Reading PIO data\n");
            insw(ports.cmd + ATA_REG_CMD_DATA,
                 (char*)io_window + io_window_misalignment,
                 (sector_size >> 1) * sub_count);
        }

        //hex_dump(data, sub_size);

        IDE_TRACE("lba = %ld\n", lba + count_base);

        data = (char*)data + sub_size;
        read_count += sub_count;
    }

    alias_window(io_window, max_multiple * sector_size, nullptr, 0);

    release_access(intr_was_enabled);

    return err;
}

isr_context_t *ide_if_t::ide_chan_t::irq_handler(int irq, isr_context_t *ctx)
{
    for (unsigned i = 0; i < ide_dev_count; ++i) {
        if (ide_devs[i].chan->ports.irq == irq)
            ide_devs[i].chan->irq_handler();
    }

    return ctx;
}

void ide_if_t::ide_chan_t::irq_handler()
{
    mutex_lock_noyield(&lock);
    io_done = 1;
    io_status = inb(ports.cmd + ATA_REG_CMD_STATUS);
    IDE_TRACE("IRQ, status=0x%x!\n", io_status);
    mutex_unlock(&lock);
    condvar_wake_one(&done_cond);
}

int ide_if_t::ide_chan_t::flush(int slave, int is_atapi)
{
    (void)slave;
    (void)is_atapi;
    return 0;
}

if_list_t ide_if_t::detect_devices()
{
    int start_at = ide_dev_count;
    if_list_t list = {
        ide_devs + start_at,
        sizeof(*ide_devs),
        0
    };

    if (bmdma_base) {
        simplex_dma = (bmdma_inb(ATA_BMDMA_REG_STATUS_n(0)) & 0x80) != 0;

        if (simplex_dma) {
            mutex_init(&dma_lock);
            condvar_init(&dma_available);
            dma_inuse = 0;
        }
    }

    for (int secondary = 0; secondary < 2; ++secondary)
        chan[secondary].detect_devices();

    list.count = ide_dev_count - start_at;

    return list;
}

void ide_dev_t::cleanup()
{
}

int64_t ide_dev_t::read_blocks(
        void *data, int64_t count, uint64_t lba)
{
    return chan->io(data, count, lba, slave, is_atapi, 1);
}

int64_t ide_dev_t::write_blocks(
        void const *data, int64_t count, uint64_t lba)
{
    return chan->io((void*)data, count, lba, slave, is_atapi, 0);
}

int ide_dev_t::flush()
{
    return chan->flush(slave, is_atapi);
}

long ide_dev_t::info(storage_dev_info_t key)
{
    switch (key) {
    case STORAGE_INFO_BLOCKSIZE:
        return !is_atapi ? 512 : 2048;

    default:
        return -1;
    }
}
