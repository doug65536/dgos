// pci driver: C=STORAGE, S=IDE

#include "dev_storage.h"
#include "ata.h"
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
#include "unique_ptr.h"
#include "inttypes.h"
#include "work_queue.h"

#define IDE_DEBUG   1
#if IDE_DEBUG
#define IDE_TRACE(...) printdbg("ide: " __VA_ARGS__)
#else
#define IDE_TRACE(...) ((void)0)
#endif

enum struct io_op_t {
    read,
    write,

    identify,
    flush
};

struct ide_chan_ports_t {
    ioport_t cmd;
    ioport_t ctl;
    uint8_t irq;
};

struct ide_if_factory_t final : public storage_if_factory_t {
    ide_if_factory_t() : storage_if_factory_t("ide") {}
    virtual std::vector<storage_if_base_t *> detect(void) override;
};

static ide_if_factory_t ide_if_factory;
STORAGE_REGISTER_FACTORY(ide_if);

struct ide_if_t final : public storage_if_base_t, public zero_init_t {
    STORAGE_IF_IMPL

    using lock_type = std::mutex;
    using scoped_lock = std::unique_lock<lock_type>;

    struct bmdma_prd_t {
        uint32_t physaddr;
        uint16_t size;
        uint16_t eot;
    } _packed;

    C_ASSERT(sizeof(bmdma_prd_t) == 8);

    //
    // Commands

    //
    // Task file

    enum struct ata_reg_cmd : uint16_t {
        DATA            = 0x00,
        FEATURES        = 0x01,   // write only
        ERROR           = 0x01,   // read only
        SECCOUNT0       = 0x02,
        LBA0            = 0x03,
        LBA1            = 0x04,
        LBA2            = 0x05,
        HDDEVSEL        = 0x06,
        COMMAND         = 0x07,   // write only
        STATUS          = 0x07    // read only
    };

    enum struct ata_reg_ctl : uint16_t {
        ALTSTATUS       = 0x00,   // read only
        CONTROL         = 0x00
    };

    struct ide_chan_t {
        ide_if_t *iface;
        int secondary;

        struct unit_data_t {
            uint64_t max_lba;
            uint32_t max_multiple;
            ata_cmd_t read_command;
            ata_cmd_t write_command;
            ata_cmd_t write_fua_command;
            ata_cmd_t flush_command;
            int8_t max_dma;
            int8_t max_ncq;
            uint8_t atapi_packet_size;
            bool old_dma;
            bool iordy;
            bool has_48bit;
            bool has_fua;
        };

        // Capabilities of master slave
        unit_data_t unit_data[2];

        ide_chan_ports_t ports;
        int busy;
        int io_done;
        int io_status;
        lock_type lock;
        std::condition_variable not_busy_cond;
        std::condition_variable done_cond;

        void *io_window;
        void *dma_bounce;
        bmdma_prd_t *dma_cur_prd;
        bmdma_prd_t *dma_full_prd;
        size_t dma_range_count;
        uintptr_t dma_prd_physaddr;
        mmphysrange_t io_window_ranges[PAGESIZE / sizeof(uint64_t)];

        ide_chan_t();
        ~ide_chan_t();

        int acquire_access();
        void release_access(int intr_was_enabled);

        void detect_devices(std::vector<storage_dev_base_t *> &list);

        // Automatically use the correct port based on the enumeration type
        _always_inline uint8_t inb(ata_reg_cmd reg);
        _always_inline uint16_t inw(ata_reg_cmd reg);
        _always_inline void outb(ata_reg_cmd reg, uint8_t value);
        _always_inline void outw(ata_reg_cmd reg, uint16_t value);
        _always_inline void outsw(ata_reg_cmd reg,
                                   void const *values, size_t count);
        _always_inline void insw(ata_reg_cmd reg, void *values, size_t count);
        _always_inline uint8_t inb(ata_reg_ctl reg);
        _always_inline void outb(ata_reg_ctl reg, uint8_t value);

        void set_drv(int slave);
        void set_lba(unit_data_t &unit, uint64_t lba);
        void set_count(unit_data_t &unit, int count);
        void set_feature(uint8_t feature, uint8_t p1 = 0, uint8_t p2 = 0,
                         uint8_t p3 = 0, uint8_t p4 = 0);
        uint8_t wait_not_bsy();
        uint8_t wait_not_bsy_and_drq();
        uint8_t wait_drq();
        void issue_command(ata_cmd_t command);
        void issue_packet_read(unit_data_t &unit, uint64_t lba, uint32_t count,
                               uint16_t burst_len, int use_dma);
        void set_irq_enable(int enable);

        void pio_read_data(unit_data_t &unit, void *buf, size_t bytes);
        void pio_write_data(unit_data_t &unit, void const *buf, size_t bytes);

        errno_t io(void *data, int64_t count, uint64_t lba,
                   int slave, int is_atapi, io_op_t op, bool fua, iocp_t *iocp);

        static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
        void irq_handler();

        errno_t flush(int slave, int is_atapi, iocp_t *iocp);

        void yield_until_irq();

        int sector_size() const;

        void trace_status(int slave, char const *msg,
                          uint8_t status);
        void trace_error(int slave, char const *msg,
                         uint8_t status, uint8_t err);
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
    lock_type dma_lock;
    std::condition_variable dma_available;
};

struct ide_dev_t : public storage_dev_base_t {
    STORAGE_DEV_IMPL

    ide_if_t::ide_chan_t *chan;
    int slave;
    int is_atapi;
};

static std::vector<ide_if_t*> ide_ifs;
static std::vector<ide_dev_t*> ide_devs;

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

std::vector<storage_if_base_t *> ide_if_factory_t::detect(void)
{
    std::vector<storage_if_base_t*> list;

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
        std::unique_ptr<ide_if_t> if_(new ide_if_t{});

        if_->chan[0].ports.cmd = pci_iter.config.is_bar_portio(0) &&
                pci_iter.config.get_bar(0)
                ? pci_iter.config.get_bar(0)
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        if_->chan[0].ports.ctl = pci_iter.config.is_bar_portio(1) &&
                pci_iter.config.get_bar(1)
                ? pci_iter.config.get_bar(1)
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        if_->chan[1].ports.cmd = pci_iter.config.is_bar_portio(2) &&
                pci_iter.config.get_bar(2)
                ? pci_iter.config.get_bar(2)
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        if_->chan[1].ports.ctl = pci_iter.config.is_bar_portio(3) &&
                pci_iter.config.get_bar(3)
                ? pci_iter.config.get_bar(3)
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        if_->bmdma_base = pci_iter.config.get_bar(4);

        if (pci_iter.config.prog_if == 0x8A ||
                pci_iter.config.prog_if == 0x80) {
            IDE_TRACE("On IRQ 14 and 15\n");
            if_->chan[0].ports.irq = 14;
            if_->chan[1].ports.irq = 15;
        } else if (pci_iter.config.irq_line != 0xFE) {
            IDE_TRACE("Both on IRQ %d\n", pci_iter.config.irq_line);
            if_->chan[0].ports.irq = pci_iter.config.irq_line;
            if_->chan[1].ports.irq = pci_iter.config.irq_line;
        } else {
            IDE_TRACE("Both on IRQ 14\n");
            if_->chan[0].ports.irq = 14;
            if_->chan[1].ports.irq = 14;
            IDE_TRACE("Updating PCI config IRQ line\n");
            pci_config_write(pci_iter.addr,
                           offsetof(pci_config_hdr_t, irq_line),
                           &if_->chan[0].ports.irq, sizeof(uint8_t));
        }

        IDE_TRACE("Found PCI IDE interface at %#x/%#x/irq%d"
                  " - %#x/%#x/irq%d\n",
                  if_->chan[0].ports.cmd,
                if_->chan[0].ports.ctl, if_->chan[0].ports.irq,
                if_->chan[1].ports.cmd, if_->chan[1].ports.ctl,
                if_->chan[1].ports.irq);

        // Sanity check ports, reject entry if any are zero
        if (if_->chan[0].ports.cmd == 0 ||
                if_->chan[0].ports.ctl == 0 ||
                if_->chan[1].ports.cmd == 0 ||
                if_->chan[1].ports.ctl == 0)
            continue;

        // Enable bus master DMA and I/O ports, disable MMIO
        pci_adj_control_bits(pci_iter, PCI_CMD_BME | PCI_CMD_IOSE, PCI_CMD_MSE);

        if (!ide_ifs.push_back(if_))
            panic_oom();
        if (!list.push_back(if_.release()))
            panic_oom();
    } while (pci_enumerate_next(&pci_iter));

    return list;
}

void ide_if_t::cleanup_if()
{
}

uint8_t ide_if_t::ide_chan_t::wait_not_bsy()
{
    uint8_t status;

    for (;; pause()) {
        status = inb(ata_reg_ctl::ALTSTATUS);

        if (!(status & ATA_REG_STATUS_BSY))
            break;

        if (unlikely(status == 0xFF))
            break;
    }

    return status;
}

uint8_t ide_if_t::ide_chan_t::wait_not_bsy_and_drq()
{
    uint8_t status;

    for (;; pause()) {
        status = inb(ata_reg_ctl::ALTSTATUS);

        if ((status & (ATA_REG_STATUS_BSY | ATA_REG_STATUS_DRQ)) ==
             ATA_REG_STATUS_DRQ)
            break;

        if (unlikely(status == 0xFF))
            break;
    }

    return status;
}

uint8_t ide_if_t::ide_chan_t::wait_drq()
{
    uint8_t status;

    for (;; pause()) {
        status = inb(ata_reg_ctl::ALTSTATUS);

        if (status & ATA_REG_STATUS_DRQ)
            break;

        if (unlikely(status == 0xFF))
            break;
    }

    return status;
}

void ide_if_t::ide_chan_t::issue_command(ata_cmd_t command)
{
    outb(ata_reg_cmd::COMMAND, uint8_t(command));
}

void ide_if_t::ide_chan_t::issue_packet_read(
        unit_data_t &unit, uint64_t lba, uint32_t count,
        uint16_t burst_len, int use_dma)
{
    outb(ata_reg_cmd::FEATURES, use_dma ? 1 : 0);
    outb(ata_reg_cmd::SECCOUNT0, 0);
    outb(ata_reg_cmd::LBA0, 0);
    outb(ata_reg_cmd::LBA1, (burst_len >> (8*0)) & 0xFF);
    outb(ata_reg_cmd::LBA2, (burst_len >> (8*1)) & 0xFF);
    set_irq_enable(0);
    issue_command(ata_cmd_t::PACKET);

    atapi_fis_t packet;

    packet.set(ATAPI_CMD_READ, lba, count, use_dma);

    wait_not_bsy_and_drq();

    set_irq_enable(1);
    pio_write_data(unit, &packet, unit.atapi_packet_size);
}

void ide_if_t::ide_chan_t::set_irq_enable(int enable)
{
    outb(ata_reg_ctl::CONTROL, ATA_REG_CONTROL_n(
             enable ? 0 : ATA_REG_CONTROL_nIEN));
}

void ide_if_t::ide_chan_t::pio_read_data(unit_data_t &unit,
                                         void *buf, size_t bytes)
{
    if (likely(unit.iordy)) {
        insw(ata_reg_cmd::DATA, buf, bytes / sizeof(uint16_t));
    } else {
        uint16_t *out = (uint16_t*)buf;

        while (bytes >= 2) {
            wait_not_bsy_and_drq();
            *out++ = inw(ata_reg_cmd::DATA);
            bytes -= 2;
        }
    }
}

void ide_if_t::ide_chan_t::pio_write_data(unit_data_t &unit,
                                          void const *buf, size_t bytes)
{
    if (likely(unit.iordy)) {
        outsw(ata_reg_cmd::DATA, buf, bytes / sizeof(uint16_t));
    } else {
        uint16_t *in = (uint16_t*)buf;
        while (bytes >= 2) {
            wait_drq();
            outw(ata_reg_cmd::DATA, *in++);
            bytes -= 2;
        }
    }
}

void ide_if_t::ide_chan_t::set_drv(int slave)
{
    outb(ata_reg_cmd::HDDEVSEL,
         ATA_REG_HDDEVSEL_n((slave ? ATA_REG_HDDEVSEL_DRV : 0) |
         ATA_REG_HDDEVSEL_LBA));
}

void ide_if_t::ide_chan_t::set_lba(unit_data_t &unit, uint64_t lba)
{
    assert(lba < (uint64_t(1) << (6*8)));

    if (unit.has_48bit) {
        outb(ata_reg_cmd::LBA0, (lba >> (3*8)) & 0xFF);
        outb(ata_reg_cmd::LBA1, (lba >> (4*8)) & 0xFF);
        outb(ata_reg_cmd::LBA2, (lba >> (5*8)) & 0xFF);
    }
    outb(ata_reg_cmd::LBA0, (lba >> (0*8)) & 0xFF);
    outb(ata_reg_cmd::LBA1, (lba >> (1*8)) & 0xFF);
    outb(ata_reg_cmd::LBA2, (lba >> (2*8)) & 0xFF);
}

void ide_if_t::ide_chan_t::set_count(unit_data_t &unit, int count)
{
    assert(count <= ((unit.has_48bit && unit.max_multiple > 1) ? 65536 : 256));
    if (unit.has_48bit)
        outb(ata_reg_cmd::SECCOUNT0, (count >> (1*8)) & 0xFF);
    outb(ata_reg_cmd::SECCOUNT0, (count >> (0*8)) & 0xFF);
}

void ide_if_t::ide_chan_t::set_feature(
        uint8_t feature, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4)
{
    outb(ata_reg_cmd::FEATURES, feature);
    outb(ata_reg_cmd::SECCOUNT0, p1);
    outb(ata_reg_cmd::LBA0, p2);
    outb(ata_reg_cmd::LBA1, p3);
    outb(ata_reg_cmd::LBA2, p4);
    issue_command(ata_cmd_t::SET_FEATURE);
}

ide_if_t::ide_chan_t::ide_chan_t()
{
}

ide_if_t::ide_chan_t::~ide_chan_t()
{
    if (dma_full_prd != nullptr) {
        munmap(dma_full_prd,
               sizeof(*dma_full_prd) * dma_range_count);
        dma_full_prd = nullptr;
    }
}

int ide_if_t::ide_chan_t::acquire_access()
{
    int intr_was_enabled = cpu_irq_save_disable();
    scoped_lock hold(lock);

    while (busy)
        not_busy_cond.wait(hold);

    busy = 1;

    return intr_was_enabled;
}

void ide_if_t::ide_chan_t::release_access(int intr_was_enabled)
{
    scoped_lock hold(lock);
    busy = 0;
    hold.unlock();
    not_busy_cond.notify_one();
    cpu_irq_toggle(intr_was_enabled);
}

void ide_if_t::ide_chan_t::detect_devices(
        std::vector<storage_dev_base_t*>& list)
{
    IDE_TRACE("Detecting IDE devices\n");

    for (int slave = 0; slave < 2; ++slave) {
        unit_data_t &unit = unit_data[slave];

        // Wait for not busy
        uint8_t status = wait_not_bsy();

        // If status == 0, no device
        if (status == 0) {
            IDE_TRACE("if=%d, slave=%d, no device\n", secondary, slave);
            continue;
        }

        trace_status(slave, "detect", status);

        // Set LBA to zero for ATAPI detection later
        set_drv(slave);
        set_lba(unit, 0);
        set_irq_enable(0);

        IDE_TRACE("if=%d, slave=%d, issuing IDENTIFY\n", secondary, slave);
        issue_command(ata_cmd_t::IDENTIFY);

        int is_atapi = 0;
        int is_err = 0;

        for (;; pause()) {
            status = inb(ata_reg_ctl::ALTSTATUS);

            // If no drive, give up
            if (status == 0)
                break;

            // Wait until not busy
            if (status & ATA_REG_STATUS_BSY)
                continue;

            // If error bit set, it failed
            if (!is_atapi && (status & ATA_REG_STATUS_ERR)) {
                uint8_t lba1 = inb(ata_reg_cmd::LBA1);
                uint8_t lba2 = inb(ata_reg_cmd::LBA2);

                // Check for ATAPI signature
                if (lba1 == 0x14 && lba2 == 0xEB)
                    is_atapi = 1;
                else if (lba1 == 0x69 && lba2 == 0x96)
                    is_atapi = 1;

                if (is_atapi) {
                    // Issue identify command
                    issue_command(ata_cmd_t::IDENTIFY_PACKET);
                    continue;
                }

                break;
            }

            // If not busy and data ready, stop spinning
            if (status & ATA_REG_STATUS_DRDY)
                break;
        }

        int log2_sector_size = !is_atapi ? 9 : 11;

        // If status == 0, no device
        if (status == 0)
            continue;

        // Check error bit in status register
        is_err = (status & ATA_REG_STATUS_ERR) != 0;

        // If error bit set, read error register
        uint8_t err = 0;
        if (is_err)
            err = inb(ata_reg_cmd::ERROR);

        // If error and command aborted, there is no drive
        if (is_err && (err & ATA_REG_ERROR_ABRT)) {
            trace_error(slave, "no drive", status, err);
            continue;
        }

        // If error, then we do not know what happened
        if (is_err) {
            trace_error(slave, "unexpected error", status, err);
            continue;
        }

        std::unique_ptr<ide_dev_t> dev(new ide_dev_t{});
        dev->chan = this;
        dev->slave = slave;
        dev->is_atapi = is_atapi;
        if (!ide_devs.push_back(dev))
            panic_oom();
        if (!list.push_back(dev.release()))
            panic_oom();

        std::unique_ptr<ata_identify_t> ident = new ata_identify_t;

        IDE_TRACE("if=%d, slave=%d, receiving IDENTIFY\n", secondary, slave);

        // Read IDENTIFY data
        memset(ident, 0, sizeof(*ident));
        pio_read_data(unit, ident, sizeof(*ident));
        //hex_dump(ident, sizeof(*ident));

        ident->fixup_strings();
        IDE_TRACE("identify:        model: %*.*s\n",
                  -(int)sizeof(ident->model),
                  (int)sizeof(ident->model),
                  ident->model);
        IDE_TRACE("identify:           fw: %*.*s\n",
                  -(int)sizeof(ident->fw_revision),
                  (int)sizeof(ident->fw_revision),
                  ident->fw_revision);
        IDE_TRACE("identify:       serial: %*.*s\n",
                  -(int)sizeof(ident->serial),
                  (int)sizeof(ident->serial),
                  ident->serial);
        IDE_TRACE("identify: media_serial: %*.*s\n",
                  -(int)sizeof(ident->media_serial),
                  (int)sizeof(ident->media_serial),
                  ident->media_serial);

        unit.max_ncq = ident->max_queue_minus1 + 1;

        unit.iordy = ident->support_iordy;

        unit.max_multiple = ident->max_multiple;

        if (is_atapi)
            unit.atapi_packet_size = ident->atapi_packet_size ? 16 : 12;

        if (unit.max_multiple == 0)
            unit.max_multiple = 1;

        unit.has_48bit = ident->support_ext48bit;

        if (!is_atapi) {
            if (unit.has_48bit)
                unit.max_lba = ident->max_lba_ext48bit;
            else
                unit.max_lba = ident->max_lba;
        } else {
            unit.max_lba = 0;
        }

        IDE_TRACE("if=%d, slave=%d, has_48bit=%d\n",
                  secondary, slave, unit.has_48bit);

        // Force dma_support to 0 if interface does not support UDMA,
        // otherwise, force dma_support to 0 if validity bit does not report
        // support for UDMA,
        // otherwise, use max supported UDMA value
        uint16_t dma_support = (iface->bmdma_base && ident->word_88_valid)
                ? ident->udma_support
                : 0;

        // Look for highest supported UDMA
        for (unit.max_dma = 5; unit.max_dma >= 0; --unit.max_dma) {
            if (dma_support & (1 << unit.max_dma))
                break;
        }

        if (unit.max_dma < 0) {
            // Fall back to old MWDMA
            dma_support = iface->bmdma_base
                    ? ident->mwdma_support
                    : 0;

            for (unit.max_dma = 2; unit.max_dma >= 0; --unit.max_dma) {
                if (dma_support & (1 << unit.max_dma))
                    break;
            }

            if (unit.max_dma >= 0)
                unit.old_dma = 1;
        }

        unit.has_fua = ident->support_fua_ext;

        ident.reset();

        IDE_TRACE("if=%d, slave=%d, max_dma=%d\n",
                  secondary, slave, unit.max_dma);

        if (unit.has_48bit) {
            if (unit.max_dma >= 0) {
                unit.read_command = ata_cmd_t::READ_DMA_EXT;
                unit.write_command = ata_cmd_t::WRITE_DMA_EXT;
                unit.write_fua_command = ata_cmd_t::WRITE_DMA_FUA_EXT;
                unit.flush_command = ata_cmd_t::CACHE_FLUSH_EXT;
                unit.max_multiple = 65536;
            } else if (unit.max_multiple > 1) {
                unit.read_command = ata_cmd_t::READ_MULT_PIO_EXT;
                unit.write_command = ata_cmd_t::WRITE_MULT_PIO_EXT;
                unit.write_fua_command = ata_cmd_t::WRITE_MULT_FUA_EXT;
                unit.flush_command = ata_cmd_t::CACHE_FLUSH_EXT;
            } else {
                unit.read_command = ata_cmd_t::READ_PIO_EXT;
                unit.write_command = ata_cmd_t::WRITE_PIO_EXT;
                unit.flush_command = ata_cmd_t::CACHE_FLUSH_EXT;
            }
        } else {
            if (unit.max_dma >= 0) {
                unit.read_command = ata_cmd_t::READ_DMA;
                unit.write_command = ata_cmd_t::WRITE_DMA;
                unit.flush_command = ata_cmd_t::CACHE_FLUSH;
                unit.max_multiple = 256;
            } else if (unit.max_multiple > 1) {
                unit.read_command = ata_cmd_t::READ_MULT_PIO;
                unit.write_command = ata_cmd_t::WRITE_MULT_PIO;
                unit.flush_command = ata_cmd_t::CACHE_FLUSH;
            } else {
                unit.read_command = ata_cmd_t::READ_PIO;
                unit.write_command = ata_cmd_t::WRITE_PIO;
                unit.flush_command = ata_cmd_t::CACHE_FLUSH;
            }
        }

        if (unit.max_dma >= 0) {
            status = wait_not_bsy();

            if (!unit.old_dma) {
                // Enable UDMA
                trace_status(slave, "enabling UDMA", status);
                set_feature(ATA_FEATURE_XFER_MODE,
                            ATA_FEATURE_XFER_MODE_UDMA_n(unit.max_dma));
            } else {
                // Enable MWDMA
                trace_status(slave, "enabling MWDMA", status);
                set_feature(ATA_FEATURE_XFER_MODE,
                            ATA_FEATURE_XFER_MODE_MWDMA_n(unit.max_dma));
            }

            // Allocate page for PRD list
            dma_cur_prd = (bmdma_prd_t*)mmap(
                        nullptr, PAGESIZE, PROT_READ | PROT_WRITE,
                        MAP_32BIT | MAP_POPULATE |
                        MAP_UNINITIALIZED, -1, 0);

            IDE_TRACE("Allocated PRD list at %p\n", (void*)dma_cur_prd);

            // Get physical address of PRD list
            //dma_prd_physaddr = mphysaddr(dma_cur_prd);

            // Allocate bounce buffer for DMA
            dma_bounce = mmap(nullptr, unit.max_multiple << log2_sector_size,
                              PROT_READ | PROT_WRITE,
                              MAP_32BIT | MAP_POPULATE |
                              MAP_UNINITIALIZED, -1, 0);
            io_window = mmap_window(unit.max_multiple << log2_sector_size);

            // Get list of physical address ranges for io_window
            dma_range_count = mphysranges(
                        io_window_ranges, countof(io_window_ranges),
                        dma_bounce, unit.max_multiple << log2_sector_size,
                        65536);

            // PRDs must not cross 64KB boundaries
            mphysranges_split(io_window_ranges, dma_range_count,
                              countof(io_window_ranges), 16);

            IDE_TRACE("PRD list has %zu entries\n", dma_range_count);

            dma_full_prd = (bmdma_prd_t*)mmap(
                        nullptr, sizeof(*dma_full_prd) * dma_range_count,
                        PROT_READ | PROT_WRITE,
                        MAP_32BIT | MAP_POPULATE, -1, 0);
            dma_prd_physaddr = mphysaddr(dma_full_prd);

            // Populate PRD list with bounce buffer addresses
            for (size_t i = 0; i < dma_range_count; ++i) {
                dma_full_prd[i].physaddr = io_window_ranges[i].physaddr;
                dma_full_prd[i].size = io_window_ranges[i].size;
                dma_full_prd[i].eot = (i == (dma_range_count-1)) ? 0x8000 : 0;
            }

            // Set PRD address register
            assert(dma_prd_physaddr && dma_prd_physaddr < 0x100000000);
            iface->bmdma_outd(ATA_BMDMA_REG_PRD_n(secondary),
                            (uint32_t)dma_prd_physaddr);
        } else if (unit.max_multiple > 1) {
            IDE_TRACE("if=%d, slave=%d, enabling MULTIPLE\n", secondary, slave);

            wait_not_bsy();
            set_count(unit, unit.max_multiple);
            set_lba(unit, 0);
            issue_command(ata_cmd_t::SET_MULTIPLE);
            status = wait_not_bsy();

            if (status & ATA_REG_STATUS_ERR) {
                IDE_TRACE("Set multiple command failed\n");
            }

            io_window = mmap_window(unit.max_multiple << log2_sector_size);
        } else {
            IDE_TRACE("if=%d, slave=%d, single sector (!)\n", secondary, slave);

            io_window = mmap_window(1 << log2_sector_size);
        }

        set_irq_enable(1);

        irq_hook(ports.irq, &ide_chan_t::irq_handler, "ide");
        irq_setmask(ports.irq, 1);

        IDE_TRACE("if=%d, slave=%d, done\n", secondary, slave);
    }
}

uint8_t ide_if_t::ide_chan_t::inb(ata_reg_cmd reg)
{
    return ::inb(ports.cmd + uint16_t(reg));
}

uint16_t ide_if_t::ide_chan_t::inw(ata_reg_cmd reg)
{
    return ::inw(ports.cmd + uint16_t(reg));
}

void ide_if_t::ide_chan_t::outb(ata_reg_cmd reg, uint8_t value)
{
    ::outb(ports.cmd + uint16_t(reg), value);
}

void ide_if_t::ide_chan_t::outw(ata_reg_cmd reg, uint16_t value)
{
    ::outw(ports.cmd + uint16_t(reg), value);
}

void ide_if_t::ide_chan_t::outsw(ata_reg_cmd reg,
                                 void const *values, size_t count)
{
    ::outsw(ports.cmd + uint16_t(reg), values, count);
}

void ide_if_t::ide_chan_t::insw(ide_if_t::ata_reg_cmd reg,
                                void *values, size_t count)
{
    ::insw(ports.cmd + uint16_t(reg), values, count);
}

uint8_t ide_if_t::ide_chan_t::inb(ide_if_t::ata_reg_ctl reg)
{
    return ::inb(ports.ctl + uint16_t(reg));
}

void ide_if_t::ide_chan_t::outb(ide_if_t::ata_reg_ctl reg, uint8_t value)
{
    ::outb(ports.ctl + uint16_t(reg), value);
}

ide_if_t::ide_if_t()
{
    chan[0].iface = this;
    chan[1].iface = this;
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
        scoped_lock hold(dma_lock);
        while (dma_inuse)
            dma_available.wait(hold);
        dma_inuse = 1;
    }
}

void ide_if_t::bmdma_release()
{
    if (simplex_dma) {
        scoped_lock hold(dma_lock);
        dma_inuse = 0;
        hold.unlock();
        dma_available.notify_one();
    }
}

void ide_if_t::ide_chan_t::yield_until_irq()
{
    scoped_lock hold(lock);
    while (!io_done)
        done_cond.wait(hold);
}

void ide_if_t::ide_chan_t::trace_status(int slave, char const *msg,
                                        uint8_t status)
{
    char status_text[64];
    format_flags_register(status_text, sizeof(status_text),
                          status, ide_flags_status);
    IDE_TRACE("%s: chan=%d, slave=%d, status=%#x (%s)\n",
              msg, secondary, slave, status, status_text);
}

void ide_if_t::ide_chan_t::trace_error(int slave,
                                       char const *msg,
                                       uint8_t status, uint8_t err)
{
    char status_text[64];
    char error_text[64];

    format_flags_register(status_text, sizeof(status_text),
                          status, ide_flags_status);

    format_flags_register(status_text, sizeof(error_text),
                          err, ide_flags_error);

    IDE_TRACE("%s: chan=%d, slave=%d, status=%#x (%s), err=%#x (%s)\n",
              msg, secondary, slave, status, status_text, err, error_text);
}

errno_t ide_if_t::ide_chan_t::io(void *data, int64_t count, uint64_t lba,
        int slave, int is_atapi, io_op_t op, bool fua, iocp_t *iocp)
{
    unit_data_t &unit = unit_data[slave];

    assert(!unit.max_lba || lba + count <= unit.max_lba);

    errno_t err = errno_t::OK;

    if (unlikely(count == 0))
        return err;

    uint8_t log2_sector_size = !is_atapi ? 9 : 11;

    IDE_TRACE("Read lba=%" PRIu64 ", max_multiple=%u, count=%" PRId64 "\n",
              lba, unit.max_multiple, count);

    int64_t read_count = 0;

    int intr_was_enabled = acquire_access();

    wait_not_bsy();
    set_drv(slave);
    set_irq_enable(1);

    for (int64_t count_base = 0;
         err == errno_t::OK && count_base < count;
         count_base += unit.max_multiple) {
        uint32_t sub_count = count - count_base;

        if (sub_count > unit.max_multiple)
            sub_count = unit.max_multiple;

        IDE_TRACE("sub_count=%u\n", sub_count);

        uint32_t sub_size = sub_count << log2_sector_size;

        {
            scoped_lock hold(lock);
            io_done = 0;
        }

        if (unit.max_dma >= 0) {
            iface->bmdma_acquire();

            // Set PRD address register
            assert(dma_prd_physaddr && dma_prd_physaddr < 0x100000000);
            iface->bmdma_outd(ATA_BMDMA_REG_PRD_n(secondary),
                            (uint32_t)dma_prd_physaddr);
        }

        if (!is_atapi) {
            set_lba(unit, lba + count_base);
            set_count(unit, sub_count);

            ata_cmd_t cmd;

            switch (op) {
            case io_op_t::read:
                cmd = unit.read_command;
                break;

            case io_op_t::write:
                cmd = (fua && unit.has_fua)
                        ? unit.write_fua_command
                        : unit.write_command;
                break;

            case io_op_t::flush:
                cmd = unit.flush_command;
                break;

            case io_op_t::identify:
                cmd = ata_cmd_t::IDENTIFY;
                break;

            default:
                cmd = ata_cmd_t::NOP;
                assert(!"Invalid op");
                break;
            }

            issue_command(cmd);
        } else {
            issue_packet_read(unit, lba + count_base, sub_count, 2048,
                              unit.max_dma >= 0);
        }

        size_t io_window_misalignment = (uintptr_t)data & ~-PAGESIZE;

        if (unit.max_dma >= 0 && op != io_op_t::flush) {
            IDE_TRACE("Starting DMA\n");

            // Program DMA reads or writes and start DMA
            iface->bmdma_outb(ATA_BMDMA_REG_CMD_n(secondary),
                              op == io_op_t::read ? 9 : 1);

            // Get physical addresses of destination
            size_t range_count = mphysranges(
                        io_window_ranges, countof(io_window_ranges),
                        data, sub_size + io_window_misalignment, ~0UL);

            // Map window to modify the memory
            alias_window(io_window, sub_size, io_window_ranges, range_count);

            if (op == io_op_t::write) {
                memcpy(dma_bounce, (char*)io_window + io_window_misalignment,
                       sub_size);
            }
        }

        yield_until_irq();

        if (io_status & ATA_REG_STATUS_ERR) {
            int ata_err = inb(ata_reg_cmd::ERROR);
            trace_error(slave, "I/O error", io_status, ata_err);
            err = errno_t::EIO;
        }

        if (unit.max_dma >= 0 && op != io_op_t::flush) {
            // Must read status register to synchronize with bus master
            IDE_TRACE("Reading DMA status\n");
            uint8_t bmdma_status = iface->bmdma_inb(
                        ATA_BMDMA_REG_STATUS_n(secondary));

            // Write 1 to interrupt bit to acknowledge
            iface->bmdma_outb(ATA_BMDMA_REG_STATUS_n(secondary),
                            bmdma_status | (1 << 2));
            IDE_TRACE("bmdma status=%#x\n", bmdma_status);

            // Stop DMA
            IDE_TRACE("Stopping DMA\n");
            iface->bmdma_outb(ATA_BMDMA_REG_CMD_n(secondary), 0);

            iface->bmdma_release();

            if (op != io_op_t::write) {
                // Copy into caller's data buffer
                memcpy((char*)io_window + io_window_misalignment,
                       dma_bounce, sub_size);
            }
        } else {
            size_t adj_size = sub_size + io_window_misalignment;
            char *aligned_addr = (char*)data - io_window_misalignment;

            size_t ranges_count = mphysranges(
                        io_window_ranges, countof(io_window_ranges),
                        aligned_addr, adj_size, ~0U);

            alias_window(io_window, sub_size,
                         io_window_ranges, ranges_count);

            IDE_TRACE("Transferring PIO data\n");
            if (op != io_op_t::flush) {
                if (op != io_op_t::write) {
                    pio_read_data(unit, (char*)io_window +
                                  io_window_misalignment,
                                  sub_count << log2_sector_size);
                } else {
                    pio_write_data(unit, (char*)io_window +
                                   io_window_misalignment,
                                   sub_count << log2_sector_size);
                }
            }
        }

        // Emulate FUA when unsupported by issuing subsequent FLUSH CACHE
        if (err == errno_t::OK && fua && !unit.has_fua) {
            // Issue FLUSH CACHE
            set_lba(unit, 0);
            set_count(unit, 0);
            outb(ata_reg_cmd::FEATURES, 0);
            issue_command(unit.flush_command);

            yield_until_irq();

            if (io_status & ATA_REG_STATUS_ERR) {
                int ata_err = inb(ata_reg_cmd::ERROR);
                trace_error(slave, "Flush I/O error", io_status, ata_err);
                err = errno_t::EIO;
            }
        }

        //hex_dump(data, sub_size);

        IDE_TRACE("lba = %" PRId64 "\n", lba + count_base);

        data = (char*)data + sub_size;
        read_count += sub_count;
    }

    alias_window(io_window, unit.max_multiple << log2_sector_size, nullptr, 0);

    release_access(intr_was_enabled);

    iocp->set_result(dgos::err_sz_pair_t{ err, size_t(count) });
    iocp->set_expect(1);
    iocp->invoke();

    return err;
}

isr_context_t *ide_if_t::ide_chan_t::irq_handler(int irq, isr_context_t *ctx)
{
    for (unsigned i = 0; i < ide_devs.size(); ++i) {
        ide_dev_t *dev = ide_devs[i];
        if (dev->chan->ports.irq == irq) {
            workq::enqueue([=] {
                dev->chan->irq_handler();
            });
        }
    }

    return ctx;
}

void ide_if_t::ide_chan_t::irq_handler()
{
    scoped_lock hold(lock);

    io_done = 1;
    io_status = inb(ata_reg_cmd::STATUS);
    hold.unlock();
    done_cond.notify_one();
}

errno_t ide_if_t::ide_chan_t::flush(int slave, int is_atapi, iocp_t *iocp)
{
    if (is_atapi) {
        // FIXME
        iocp->invoke();
        return errno_t::OK;
    }

    (void)slave;
    // FIXME
    iocp->invoke();
    return errno_t::OK;
}

std::vector<storage_dev_base_t*> ide_if_t::detect_devices()
{
    std::vector<storage_dev_base_t*> list;

    if (bmdma_base) {
        simplex_dma = (bmdma_inb(ATA_BMDMA_REG_STATUS_n(0)) & 0x80) != 0;

        if (simplex_dma) {
            scoped_lock hold(dma_lock);
            dma_inuse = 0;
        }
    }

    for (int secondary = 0; secondary < 2; ++secondary)
        chan[secondary].detect_devices(list);

    return list;
}

void ide_dev_t::cleanup_dev()
{
}

errno_t ide_dev_t::read_async(
        void *data, int64_t count, uint64_t lba,
        iocp_t *iocp)
{
    return chan->io(data, count, lba, slave, is_atapi,
                    io_op_t::read, false, iocp);
}

errno_t ide_dev_t::write_async(
        void const *data, int64_t count, uint64_t lba, bool fua,
        iocp_t *iocp)
{
    return chan->io((void*)data, count, lba, slave, is_atapi,
                    io_op_t::write, fua, iocp);
}

errno_t ide_dev_t::trim_async(int64_t count, uint64_t lba,
                              iocp_t *)
{
    (void)count;
    (void)lba;
    return errno_t::ENOSYS;
}

errno_t ide_dev_t::flush_async(iocp_t *iocp)
{
    return chan->flush(slave, is_atapi, iocp);
}

long ide_dev_t::info(storage_dev_info_t key)
{
    switch (key) {
    case STORAGE_INFO_BLOCKSIZE:
        return !is_atapi ? 512 : 2048;

    case STORAGE_INFO_HAVE_TRIM:
        return 0;

    case STORAGE_INFO_NAME:
        return long("IDE");

    default:
        return -1;
    }
}
