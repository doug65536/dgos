#include "usb_storage.h"
#include "dev_storage.h"
#include "dev_usb_ctl.h"
#include "bswap.h"
#include "hash_table.h"
#include "algorithm.h"

// USB mass storage class driver

#define DEBUG_USB_MSC 0
#if DEBUG_USB_MSC
#define USB_MSC_TRACE(...) printdbg("usbmsc: " __VA_ARGS__)
#else
#define USB_MSC_TRACE(...) ((void)0)
#endif

class usb_msc_if_factory_t;
class usb_msc_if_t;
class usb_msc_dev_t;
class usb_msc_classdrv_t;

enum usb_msc_request_t : uint8_t {
    GET_MAX_LUN = 0xFE,
    RESET = 0xFF
};

enum struct usb_msc_op_t {
    write,
    read,
    flush,
    trim
};

// A factory that enumerates all of the available storage devices
class usb_msc_if_factory_t final : public storage_if_factory_t {
public:
    usb_msc_if_factory_t();
protected:
    // storage_if_factory_t interface
    std::vector<storage_if_base_t *> detect() override;
};

// A USB storage interface
class usb_msc_if_t final : public storage_if_base_t {
    // storage_if_base_t interface
public:
    enum cmd_op_t : uint8_t {
        cmd_read_capacity_10 = 0x25,
        cmd_read_capacity_16 = 0x9E,
        cmd_read_10 = 0x28,
        cmd_read_12 = 0xA8,
        cmd_read_16 = 0x88,
        cmd_write_10 = 0x2A,
        cmd_write_12 = 0xAA,
        cmd_write_16 = 0x8A
    };

    // read capacity 16
    struct cmd_rdcap_16_t {
        cmd_op_t op;  // 0x9E
        uint8_t serv_act;   // 0x10
        uint64_t lba;
        uint32_t alloc;
        uint8_t pmi;
        uint8_t control;
    } _packed;

    C_ASSERT(sizeof(cmd_rdcap_16_t) == 16);

    // read capacity 10
    struct cmd_rdcap_10_t {
        cmd_op_t op;  // 0x25
        uint8_t reserved;
        uint32_t lba;
        uint16_t reserved2;
        uint8_t pmi;
        uint8_t control;
    } _packed;

    C_ASSERT(sizeof(cmd_rdcap_10_t) == 10);

    struct rdcap_10_data_t {
        uint32_t lba;
        uint32_t block_sz;
    } _packed;

    struct cmd_rw16_t {
        cmd_op_t op;     // read=0x88, write=0x8A
        uint8_t flags;  // bits: 1=fua_nv, 3=fua, 4=dpo, 7:5 protect
        uint64_t lba;
        uint32_t len;
        uint8_t group;
        uint8_t control;
    } _packed;

    C_ASSERT(sizeof(cmd_rw16_t) == 16);

    struct cmd_rw12_t {
        cmd_op_t op;    // read=0xA8, write=0xAA
        uint8_t flags;  // bits: 1=fua_nv, 3=fua, 4=dpo, 7:5=protect
        uint32_t lba;
        uint32_t len;
        uint8_t group;
        uint8_t control;
    } _packed;

    C_ASSERT(sizeof(cmd_rw12_t) == 12);

    struct cmd_rw10_t {
        uint8_t op;     // read=0x28
        uint8_t flags;  // bits: 1=fua_nv, 3=fua, 4=dpo, 7:5=rdprotect
        uint32_t lba;
        uint8_t group;
        uint16_t len;
        uint8_t control;
    } _packed;

    C_ASSERT(sizeof(cmd_rw10_t) == 10);

    union cmdblk_t {
        uint8_t raw[16];
        cmd_rdcap_10_t rdcap10;
        cmd_rdcap_16_t rdcap16;
        cmd_rw10_t rw10;
        cmd_rw12_t rw12;
        cmd_rw16_t rw16;
    } _packed;

    C_ASSERT(sizeof(cmdblk_t) == 16);

    // Command block wrapper
    struct cbw_t {
        uint32_t sig;
        uint32_t tag;
        uint32_t xfer_len;
        uint8_t flags;
        uint8_t lun;
        uint8_t wcb_len;
        cmdblk_t cmd;
    } _packed;

    C_ASSERT(sizeof(cbw_t) == 31);

    static constexpr uint32_t cbw_sig = 0x43425355;

    enum struct cmd_status_t : uint8_t {
        success,
        failed,
        phase_err
    };

    // Command status wrapper
    struct csw_t {
        uint32_t sig;
        uint32_t tag;
        uint32_t residue;
        cmd_status_t status;
    } _packed;

    C_ASSERT(sizeof(csw_t) == 13);

    static constexpr uint32_t csw_sig = 0x53425255;

    union wrapper_t {
        cbw_t cbw;
        csw_t csw;
    };

    struct rdcap_10_response_t {
        uint32_t max_lba;
        uint32_t blk_size;
    } _packed;

    struct rdcap_16_response_t {
        uint64_t max_lba;
        uint32_t blk_size;
        // Rest of stuff not used and not transferred
    } _packed;

    union rdcap_wrapper_t {
        rdcap_10_response_t rdcap10;
        rdcap_16_response_t rdcap16;
    } _packed;

    struct pending_cmd_t {
        pending_cmd_t()
            : io_iocp(&usb_msc_if_t::usb_completion, uintptr_t(this))
        {
        }

        cbw_t cbw;
        csw_t csw;

        usb_msc_if_t *if_;
        iocp_t *caller_iocp;
        usb_iocp_t io_iocp;
        void *data;
        uint32_t tag;
        uint32_t lba;
        uint32_t count;
        uint8_t lun;
        uint8_t log2_sector_sz;
        usb_msc_op_t op;
    };

    // Commands are enqueued at head from I/O requests
    // Next command is dequeued at tail for issue to device
    pending_cmd_t *cmd_queue;
    uint32_t cmd_head;
    uint32_t cmd_tail;
    static constexpr uint32_t cmd_capacity =
            std::min(PAGE_SIZE / sizeof(pending_cmd_t), size_t(32U));

    using lock_type = ext::noirq_lock<ext::spinlock>;
    using scoped_lock = std::unique_lock<lock_type>;
    lock_type cmd_lock;
    std::condition_variable cmd_cond;

    static uint32_t cmd_wrap(uint32_t n);

    bool init(usb_pipe_t const& control,
              usb_pipe_t const& bulk_in,
              usb_pipe_t const& bulk_out,
              int iface_idx);

    errno_t io(void *data, uint64_t count, uint64_t lba,
               bool fua, usb_msc_op_t op, iocp_t *iocp,
               int lun, uint8_t log2_sectorsize);

    void wait_cmd_not_full(scoped_lock &hold_cmd_lock);

    static void usb_completion(usb_iocp_result_t const& result, uintptr_t arg);
    void usb_completion(pending_cmd_t *cmd);

    void issue_cmd(pending_cmd_t *cmd, scoped_lock &hold_cmd_lock);

    int get_max_lun();
    int reset();

private:
    STORAGE_IF_IMPL

    usb_pipe_t control, bulk_in, bulk_out;
    int iface_idx;
    int next_tag;
};

// A LUN on a storage interface
class usb_msc_dev_t : public storage_dev_base_t {
public:
    usb_msc_dev_t();
    ~usb_msc_dev_t();

    bool init(usb_msc_if_t *if_, int lun,
              uint64_t max_lba, uint8_t log2_blk_sz);

protected:
    // storage_dev_base_t interface
    STORAGE_DEV_IMPL

    errno_t io(void *data, uint64_t count,
               uint64_t lba, bool fua,
               usb_msc_op_t op, iocp_t *iocp);

    int flush() override final;
    int trim();

    usb_msc_if_t *if_;
    uint64_t max_lba;
    uint8_t log2_blk_size;
    uint8_t lun;
};

// USB mass storage class driver
class usb_msc_classdrv_t : public usb_class_drv_t {
public:

protected:
    // usb_class_drv_t interface
    bool probe(usb_config_helper *cfg_hlp, usb_bus_t *bus) override final;

    char const *name() const override final;
};

static std::vector<usb_msc_if_t*> usb_msc_devices;
static std::vector<usb_msc_dev_t*> usb_msc_drives;

//
// Storage interface factory

usb_msc_if_factory_t::usb_msc_if_factory_t()
    : storage_if_factory_t("usb_msc")
{
    storage_if_register_factory(this);
}

std::vector<storage_if_base_t *> usb_msc_if_factory_t::detect()
{
    USB_MSC_TRACE("Reporting %d USB mass storage interfaces\n", usb_msc_count);

    std::vector<storage_if_base_t*> list(usb_msc_devices.begin(),
                                         usb_msc_devices.end());
    return list;
}

//
// Storage interface

void usb_msc_if_t::cleanup_if()
{
}

std::vector<storage_dev_base_t*> usb_msc_if_t::detect_devices()
{
    std::vector<storage_dev_base_t*> list(usb_msc_drives.begin(),
                                     usb_msc_drives.end());

    USB_MSC_TRACE("Reporting %d USB mass storage drives\n",
                  list.size());

    return list;
}

//
// USB device (LUN)

void usb_msc_dev_t::cleanup_dev()
{
}

errno_t usb_msc_dev_t::cancel_io(iocp_t *iocp)
{
    return errno_t::ENOSYS;
}

errno_t usb_msc_dev_t::read_async(
        void *data, int64_t count,
        uint64_t lba, iocp_t *iocp)
{
    //USB_MSC_TRACE("Reading %" PRId64 " blocks at LBA %#" PRIx64, count, lba);

    errno_t status = io(data, count, lba, false,
                        usb_msc_op_t::read, iocp);
    iocp->set_expect(1);
    return status;
}

errno_t usb_msc_dev_t::write_async(
        void const *data, int64_t count,
        uint64_t lba, bool fua, iocp_t *iocp)
{
    errno_t status = io((void*)data, count, lba, fua,
                        usb_msc_op_t::write, iocp);
    iocp->set_expect(1);
    return status;
}

errno_t usb_msc_dev_t::flush_async(iocp_t *iocp)
{
    return io(nullptr, 0, 0, false, usb_msc_op_t::flush, iocp);
}

errno_t usb_msc_dev_t::trim_async(int64_t count,
        uint64_t lba, iocp_t *iocp)
{
    return io(nullptr, count, lba, false, usb_msc_op_t::trim, iocp);
}

long usb_msc_dev_t::info(storage_dev_info_t key)
{
    switch (key) {
    case STORAGE_INFO_BLOCKSIZE:
        return 1L << log2_blk_size;

    case STORAGE_INFO_HAVE_TRIM:
        return 0;

    case STORAGE_INFO_NAME:
        return long("USB-MSC");

    default:
        return 0;
    }
}

//
// USB mass storage class driver

bool usb_msc_classdrv_t::probe(usb_config_helper *cfg_hlp, usb_bus_t *bus)
{
    // Match SCSI mass storage devices
    // 6 = SCSI command set
    // 0x50 = bulk only
    match_result match = match_config(
                cfg_hlp, 0, int(usb_class_t::mass_storage), 6, 0x50,
                -1, -1, -1);

    if (!match.dev)
        return false;

    USB_MSC_TRACE("found USB mass storage interface, slot=%d\n",
                  cfg_hlp->slot());

    usb_pipe_t control;
    usb_pipe_t bulk_in;
    usb_pipe_t bulk_out;
    uint8_t iface_idx = match.iface_idx;

    if (!bus->get_pipe(cfg_hlp->slot(), 0, control))
        return false;

    assert(control);

    usb_desc_ep const *ep = nullptr;

    for (int i = 0; (ep = cfg_hlp->find_ep(match.iface, i)) != nullptr; ++i) {
        usb_pipe_t &pipe = ep->ep_addr >= 0x80 ? bulk_in : bulk_out;
        if (!bus->alloc_pipe(cfg_hlp->slot(), match.iface, ep, pipe))
            return false;
    }

    assert(bulk_in);
    assert(bulk_out);

    // Allocate an interface
    std::unique_ptr<usb_msc_if_t> if_(new (std::nothrow) usb_msc_if_t{});

    USB_MSC_TRACE("initializing interface, slot=%d\n",
                  cfg_hlp->slot());

    int status = if_->init(control, bulk_in, bulk_out, iface_idx);

    if (status < 0)
        return false;

    if (likely(usb_msc_devices.push_back(if_.get())))
        if_.release();

    return true;
}

char const *usb_msc_classdrv_t::name() const
{
    return "USB mass storage (bulk)";
}

usb_msc_dev_t::usb_msc_dev_t()
{

}

usb_msc_dev_t::~usb_msc_dev_t()
{

}

bool usb_msc_dev_t::init(usb_msc_if_t *if_, int lun,
                         uint64_t max_lba, uint8_t log2_blk_sz)
{
    this->if_ = if_;
    this->lun = lun;

    this->log2_blk_size = log2_blk_sz;
    this->max_lba = max_lba;

    return true;
}

errno_t usb_msc_dev_t::io(void *data, uint64_t count, uint64_t lba,
                          bool fua, usb_msc_op_t op, iocp_t *iocp)
{
    return if_->io(data, count, lba, fua, op, iocp, lun, log2_blk_size);
}

int usb_msc_dev_t::flush()
{
    // Any way to flush?
    return 0;
}

int usb_msc_dev_t::trim()
{
    return -int(errno_t::ENOSYS);
}

static usb_msc_classdrv_t usb_mass_storage;

uint32_t usb_msc_if_t::cmd_wrap(uint32_t n)
{
    return n < cmd_capacity ? n : 0;
}

bool usb_msc_if_t::init(usb_pipe_t const& control,
                        usb_pipe_t const& bulk_in,
                        usb_pipe_t const& bulk_out, int iface_idx)
{
    this->control = control;
    this->bulk_in = bulk_in;
    this->bulk_out = bulk_out;
    this->iface_idx = iface_idx;

    // Allocate a page for the command queue
    cmd_queue = (pending_cmd_t*)mmap(nullptr, PAGE_SIZE,
                                     PROT_READ | PROT_WRITE, MAP_POPULATE);
    cmd_head = 0;
    cmd_tail = 0;

    // Initialize command queue entries
    for (uint32_t i = 0; i < cmd_capacity; ++i)
        new (cmd_queue + i) pending_cmd_t();

    USB_MSC_TRACE("Resetting interface\n");

    reset();

    USB_MSC_TRACE("Getting max LUN\n");

    int max_lun = get_max_lun();

    USB_MSC_TRACE("max_lun=%d\n", max_lun);

    if (max_lun < 0)
        return false;

    for (int lun = 0; lun <= max_lun; ++lun) {
        USB_MSC_TRACE("Initializing lun %d\n", lun);

        std::unique_ptr<usb_msc_dev_t> drv(
                    new (std::nothrow) usb_msc_dev_t{});

        // Get size and block size
        wrapper_t cap;
        rdcap_wrapper_t rdcap;

        cap.cbw = cbw_t{};
        cap.cbw.lun = lun;
        cap.cbw.sig = cbw_sig;
        cap.cbw.wcb_len = sizeof(cap.cbw.cmd.rdcap10);
        cap.cbw.flags = 0x80;
        cap.cbw.xfer_len = sizeof(rdcap.rdcap10);
        cap.cbw.cmd.rdcap10.op = cmd_read_capacity_10;
        bulk_out.send(&cap.cbw, sizeof(cap.cbw));

        rdcap.rdcap10 = rdcap_10_response_t{};
        bulk_in.recv(&rdcap.rdcap10, sizeof(rdcap.rdcap10));

        cap.csw = csw_t{};
        bulk_in.recv(&cap.csw, sizeof(cap.csw));

        uint64_t max_lba = 0;
        uint32_t blk_sz = 0;

        if (rdcap.rdcap10.max_lba != 0xFFFFFFFF) {
            max_lba = bswap_32(rdcap.rdcap10.max_lba);
            blk_sz = bswap_32(rdcap.rdcap10.blk_size);
        } else {
            cap.cbw = cbw_t{};
            cap.cbw.lun = lun;
            cap.cbw.sig = cbw_sig;
            cap.cbw.wcb_len = sizeof(cap.cbw.cmd.rdcap16);
            cap.cbw.xfer_len = sizeof(rdcap.rdcap16);
            cap.cbw.cmd.rdcap16.op = cmd_read_capacity_16;
            cap.cbw.cmd.rdcap16.serv_act = 0x10;
            cap.cbw.cmd.rdcap16.alloc = sizeof(rdcap.rdcap16);

            bulk_out.send(&cap.cbw, sizeof(cap.cbw));
            bulk_in.recv(&rdcap.rdcap10, sizeof(rdcap.rdcap10));
            bulk_in.recv(&cap.csw, sizeof(cap.csw));
            max_lba = bswap_64(rdcap.rdcap16.max_lba);
            blk_sz = bswap_32(rdcap.rdcap16.blk_size);
        }

        uint8_t log2_blk_sz = bit_msb_set(blk_sz);

        if (drv->init(this, lun, max_lba, log2_blk_sz)) {
            if (likely(usb_msc_drives.push_back(drv.get())))
                drv.release();
        }

        USB_MSC_TRACE("initializing lun %d complete\n", lun);
    }

    USB_MSC_TRACE("interface initialization complete\n");

    return true;
}

errno_t usb_msc_if_t::io(void *data, uint64_t count, uint64_t lba,
                         bool fua, usb_msc_op_t op, iocp_t *iocp,
                         int lun, uint8_t log2_sectorsize)
{
    scoped_lock hold_cmd_lock(cmd_lock);

    wait_cmd_not_full(hold_cmd_lock);

    // Get a pointer to the next available entry in the command queue
    pending_cmd_t *cmd = cmd_queue + cmd_head;

    USB_MSC_TRACE("Enqueueing command at slot %u\n", cmd_head);

    // Compute number of transfers in chunks of up to 64KB
    uint64_t sz = count << log2_sectorsize;
    int tx_count = (sz + (1 << 16) - 1) >> 16;

    // FIXME: handle multiple chunks
    assert(tx_count == 1);

    cmd->if_ = this;
    cmd->io_iocp.reset(&usb_msc_if_t::usb_completion);
    cmd->caller_iocp = iocp;
    cmd->data = data;
    cmd->tag = atomic_xadd(&next_tag, tx_count);
    cmd->lba = lba;
    cmd->count = count;
    cmd->lun = lun;
    cmd->log2_sector_sz = log2_sectorsize;
    cmd->op = op;

    // Advance command queue head
    cmd_head = cmd_wrap(cmd_head + 1);

    issue_cmd(cmd, hold_cmd_lock);

    return errno_t::OK;
}

void usb_msc_if_t::wait_cmd_not_full(scoped_lock& hold_cmd_lock)
{
    while (cmd_wrap(cmd_head + 1) == cmd_tail) {
        USB_MSC_TRACE("Waiting for full queue\n");
        cmd_cond.wait(hold_cmd_lock);
        USB_MSC_TRACE("Wait for full queue completed\n");
    }
}

void usb_msc_if_t::usb_completion(
        usb_iocp_result_t const& result, uintptr_t arg)
{
    pending_cmd_t *cmd = reinterpret_cast<pending_cmd_t*>(arg);
    cmd->if_->usb_completion(cmd);
}

void usb_msc_if_t::issue_cmd(pending_cmd_t *cmd, scoped_lock& hold_cmd_lock)
{
    // command block wrapper

    cmd->io_iocp.reset(&usb_msc_if_t::usb_completion);

    cmd->cbw.sig = cbw_sig;
    cmd->cbw.tag = cmd->tag;
    cmd->cbw.xfer_len = cmd->count << cmd->log2_sector_sz;

    if (cmd->op == usb_msc_op_t::read)
        cmd->cbw.flags = 0x80;
    else
        cmd->cbw.flags = 0;

    cmd->cbw.lun = cmd->lun;

    cmd->cbw.wcb_len = sizeof(cmd->cbw.cmd.rw10);
    cmd->cbw.cmd.rw10.op = cmd_read_10;
    cmd->cbw.cmd.rw10.flags = 0;
    cmd->cbw.cmd.rw10.lba = bswap_32(cmd->lba);
    cmd->cbw.cmd.rw10.len = bswap_16(cmd->count);
    cmd->cbw.cmd.rw10.group = 0;
    cmd->cbw.cmd.rw10.control = 0;

    bulk_out.send_async(&cmd->cbw, sizeof(cmd->cbw), &cmd->io_iocp);

    if (cmd->count) {
        // data transfer

        if (cmd->op == usb_msc_op_t::read) {
            bulk_in.recv_async(cmd->data, cmd->count << cmd->log2_sector_sz,
                               &cmd->io_iocp);
        } else {
            bulk_out.send_async(cmd->data, cmd->count << cmd->log2_sector_sz,
                                &cmd->io_iocp);
        }
    }

    // status block wrapper
    bulk_in.recv_async(&cmd->csw, sizeof(cmd->csw), &cmd->io_iocp);

    cmd->io_iocp.set_expect(3);
}

void usb_msc_if_t::usb_completion(pending_cmd_t *cmd)
{
    auto xferred = cmd->cbw.xfer_len - cmd->csw.residue;

    if (cmd->csw.status == cmd_status_t::success) {
        USB_MSC_TRACE("Command completed successfully\n");
        cmd->caller_iocp->set_result({ errno_t::OK, xferred });
    } else {
        USB_MSC_TRACE("Command failed \n");
        cmd->caller_iocp->set_result({ errno_t::EIO, xferred });
    }

    // Entry is finished
    scoped_lock hold_cmd_lock(cmd_lock);
    cmd_tail = cmd_wrap(cmd_tail + 1);
    cmd_cond.notify_one();

    iocp_t *caller_iocp = cmd->caller_iocp;

    hold_cmd_lock.unlock();

    caller_iocp->invoke();
}

int usb_msc_if_t::usb_msc_if_t::get_max_lun()
{
    uint8_t max_lun;

    int ncc = control.send_default_control(
                uint8_t(usb_dir_t::IN) |
                (uint8_t(usb_req_type::CLASS) << 5) |
                uint8_t(usb_req_recip_t::INTERFACE),
                uint8_t(usb_msc_request_t::GET_MAX_LUN),
                0, iface_idx, sizeof(max_lun), &max_lun);

    if (ncc == -int(usb_cc_t::stall_err)) {
        // Not supported, assume 1
        return 1;
    }

    return ncc >= 0 ? max_lun : ncc;
}

int usb_msc_if_t::reset()
{
    if (control.send_default_control(
                uint8_t(usb_dir_t::OUT) |
                (uint8_t(usb_req_type::CLASS) << 5) |
                uint8_t(usb_req_recip_t::INTERFACE),
                uint8_t(usb_msc_request_t::RESET),
                0, iface_idx, 0, nullptr) < 0)
        return false;

    control.clear_ep_halt(bulk_in);
    control.clear_ep_halt(bulk_out);

    return true;
}

static usb_msc_if_factory_t usb_msc_if_factory;
