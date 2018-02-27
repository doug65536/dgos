#include "usb_storage.h"
#include "dev_storage.h"
#include "dev_usb_ctl.h"
#include "bswap.h"
#include "hash_table.h"

// USB mass storage class driver

#define DEBUG_USB_MSC 1
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
class usb_msc_if_factory_t : public storage_if_factory_t {
public:
    usb_msc_if_factory_t() : storage_if_factory_t("usb_msc") {}
protected:
    // storage_if_factory_t interface
    if_list_t detect() override;
};

// A USB storage interface
class usb_msc_if_t : public storage_if_base_t {
    // storage_if_base_t interface
public:
    STORAGE_IF_IMPL

    enum cmd_op_t : uint8_t {
        cmd_read_capacity_10 = 0x25,
        cmd_read_12 = 0xA8,
        cmd_write_12 = 0xAA,
        cmd_read_16 = 0x88,
        cmd_write_16 = 0x8A
    };

    // read capacity 10
    struct cmd_rdcap_10_t {
        cmd_op_t op;  // 0x25
        uint8_t reserved;
        uint32_t lba;
        uint16_t reserved2;
        uint8_t pmi;
        uint8_t control;
    } __packed;

    C_ASSERT(sizeof(cmd_rdcap_10_t) == 10);

    struct rdcap_10_data_t {
        uint32_t lba;
        uint32_t block_sz;
    } __packed;

    struct cmd_rw16_t {
        cmd_op_t op;     // read=0x88, write=0x8A
        uint8_t flags;  // bits: 1=fua_nv, 3=fua, 4=dpo, 7:5 protect
        uint64_t lba;
        uint32_t len;
        uint8_t group;
        uint8_t control;
    } __packed;

    C_ASSERT(sizeof(cmd_rw16_t) == 16);

    struct cmd_rw12_t {
        cmd_op_t op;     // read=0xA8, write=0xAA
        uint8_t flags;  // bits: 1=fua_nv, 3=fua, 4=dpo, 7:5 protect
        uint32_t lba;
        uint32_t len;
        uint8_t group;
        uint8_t control;
    } __packed;

    C_ASSERT(sizeof(cmd_rw12_t) == 12);

    union cmdblk_t {
        uint8_t raw[16];
        cmd_rdcap_10_t rdcap10;
        cmd_rw12_t rw12;
        cmd_rw16_t rw16;
    } __packed;

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
    } __packed;

    C_ASSERT(sizeof(cbw_t) == 31);

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
    } __packed;

    C_ASSERT(sizeof(csw_t) == 13);

    union wrapper_t {
        cbw_t cbw;
        csw_t csw;
    };

    struct pending_cmd_t {
        pending_cmd_t()
            : io_iocp(&usb_msc_if_t::usb_completion, uintptr_t(this))
        {
        }

        enum struct phase_t : uint8_t {
            tx_cbw,
            xfer,
            rx_sbw,
            check
        };

        wrapper_t packet;

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
        phase_t phase;
    };

    pending_cmd_t *cmd_queue;
    uint32_t cmd_head;
    uint32_t cmd_tail;
    static constexpr uint32_t cmd_capacity =
            PAGE_SIZE / sizeof(pending_cmd_t);
    ticketlock cmd_lock;
    condition_variable cmd_cond;

    static uint32_t cmd_wrap(uint32_t n);

    bool init(usb_pipe_t const& control,
              usb_pipe_t const& bulk_in,
              usb_pipe_t const& bulk_out,
              int iface_idx);

    errno_t io(void *data, uint64_t count, uint64_t lba,
               bool fua, usb_msc_op_t op, iocp_t *iocp,
               int lun, uint8_t log2_sectorsize);

    static void usb_completion(usb_iocp_result_t const& result, uintptr_t arg);
    void usb_completion(pending_cmd_t *cmd);

    bool advance_cmd(pending_cmd_t *cmd);

    int get_max_lun();
    int reset();

    hashtbl_t pending_cmds;

    usb_pipe_t control, bulk_in, bulk_out;
    int iface_idx;
    int next_tag;
};

// A LUN on a storage interface
class usb_msc_dev_t : public storage_dev_base_t {
public:
    usb_msc_dev_t();
    ~usb_msc_dev_t();

    bool init(usb_msc_if_t *if_, int lun);

protected:
    // storage_dev_base_t interface
    STORAGE_DEV_IMPL

    errno_t io(void *data, uint64_t count,
               uint64_t lba, bool fua,
               usb_msc_op_t op, iocp_t *iocp);

    int flush();
    int trim();

    usb_msc_if_t *if_;
    uint8_t lun;

    uint8_t log2_sectorsize;
};

// USB mass storage class driver
class usb_msc_classdrv_t : public usb_class_drv_t {
public:


protected:
    // usb_class_drv_t interface
    bool probe(usb_config_helper *cfg_hlp, usb_bus_t *bus) override final;
};

#define USB_MSC_MAX_DEVICES    16
static usb_msc_if_t usb_msc_devices[USB_MSC_MAX_DEVICES];
static unsigned usb_msc_count;

#define USB_MSC_MAX_DRIVES    64
static usb_msc_dev_t usb_msc_drives[USB_MSC_MAX_DRIVES];
static unsigned usb_msc_drive_count;

//
// Storage interface factory

if_list_t usb_msc_if_factory_t::detect()
{
    if_list_t list = {
        usb_msc_devices,
        sizeof(*usb_msc_devices),
        usb_msc_count
    };

    return list;
}

//
// Storage interface

void usb_msc_if_t::cleanup()
{
}

if_list_t usb_msc_if_t::detect_devices()
{
    if_list_t list = {
        usb_msc_drives,
        sizeof(*usb_msc_drives),
        usb_msc_drive_count
    };

    return list;
}

//
// USB device (LUN)

void usb_msc_dev_t::cleanup()
{
}

errno_t usb_msc_dev_t::read_async(
        void *data, int64_t count,
        uint64_t lba, iocp_t *iocp)
{
    //USB_MSC_TRACE("Reading %ld blocks at LBA %lx", count, lba);
    iocp->set_expect(1);

    return io(data, count, lba, false, usb_msc_op_t::read, iocp);
}

errno_t usb_msc_dev_t::write_async(
        void const *data, int64_t count,
        uint64_t lba, bool fua, iocp_t *iocp)
{
    iocp->set_expect(1);
    return io((void*)data, count, lba, fua, usb_msc_op_t::write, iocp);
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
        return 1L << log2_sectorsize;

    case STORAGE_INFO_HAVE_TRIM:
        return 0;

    default:
        return 0;
    }
}

//
// USB mass storage class driver

bool usb_msc_classdrv_t::probe(usb_config_helper *cfg_hlp, usb_bus_t *bus)
{
    // Match SCSI mass storage devices
    match_result match = match_config(
                cfg_hlp, 0, int(usb_class_t::mass_storage), -1, -1, -1);

    if (!match.dev)
        return false;

    usb_pipe_t control;
    usb_pipe_t bulk_in;
    usb_pipe_t bulk_out;
    uint8_t iface_idx = match.iface_idx;

    if (!bus->get_pipe(cfg_hlp->slot(), 0, control))
        return false;

    usb_desc_ep const *ep = nullptr;

    for (int i = 0; (ep = cfg_hlp->find_ep(match.iface, i)) != nullptr; ++i) {
        usb_pipe_t &pipe = ep->ep_addr >= 0x80 ? bulk_in : bulk_out;
        if (!bus->alloc_pipe(cfg_hlp->slot(), ep->ep_addr, pipe,
                             ep->max_packet_sz, ep->interval, ep->ep_attr))
            return false;
    }

    if (usb_msc_count == countof(usb_msc_devices)) {
        USB_MSC_TRACE("Too many USB mass storage interfaces! Dropped one\n");
        return false;
    }

    // Allocate an interface
    usb_msc_if_t *if_ = usb_msc_devices + usb_msc_count++;

    int status = if_->init(control, bulk_in, bulk_out, iface_idx);

    return status >= 0;
}

usb_msc_dev_t::usb_msc_dev_t()
{

}

usb_msc_dev_t::~usb_msc_dev_t()
{

}

bool usb_msc_dev_t::init(usb_msc_if_t *if_, int lun)
{
    this->if_ = if_;
    this->lun = lun;

    // hack!
    log2_sectorsize = 9;

    return true;
}

errno_t usb_msc_dev_t::io(void *data, uint64_t count, uint64_t lba,
                          bool fua, usb_msc_op_t op, iocp_t *iocp)
{
    return if_->io(data, count, lba, fua, op, iocp, lun, log2_sectorsize);
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
    cmd_queue = (pending_cmd_t*)mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                                     MAP_POPULATE, -1, 0);
    cmd_head = 0;
    cmd_tail = 0;

    // Initialize command queue entries
    for (uint32_t i = 0; i < cmd_capacity; ++i)
        new (cmd_queue + i) pending_cmd_t();

    // Initialize pending command table
    htbl_create(&pending_cmds, offsetof(pending_cmd_t, tag),
                sizeof(pending_cmd_t::tag));

    reset();

    int max_lun = get_max_lun();

    if (max_lun < 0)
        return false;

    for (int lun = 0; lun <= max_lun; ++lun) {
        if (usb_msc_drive_count >= countof(usb_msc_drives)) {
            USB_MSC_TRACE("Too many drives! Dropped one\n");
            continue;
        }

        usb_msc_dev_t *drv = usb_msc_drives + usb_msc_drive_count++;
        drv->init(this, lun);
    }

    return true;
}

errno_t usb_msc_if_t::io(void *data, uint64_t count, uint64_t lba,
                         bool fua, usb_msc_op_t op, iocp_t *iocp,
                         int lun, uint8_t log2_sectorsize)
{
    unique_lock<ticketlock> hold_cmd_lock(cmd_lock);

    // Wait for room in the command queue
    while (cmd_wrap(cmd_head + 1) == cmd_tail)
        cmd_cond.wait(hold_cmd_lock);

    // Get a pointer to the next available entry in the command queue
    pending_cmd_t *cmd = cmd_queue + cmd_head;

    // Advance command queue head
    cmd_head = cmd_wrap(cmd_head + 1);

    // Compute number of transfers in chunks of up to 64KB
    uint64_t sz = count << log2_sectorsize;
    int tx_count = (sz + (1 << 16) - 1) >> 16;

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
    cmd->phase = pending_cmd_t::phase_t::tx_cbw;

    advance_cmd(cmd);

    return errno_t::OK;
}

void usb_msc_if_t::usb_completion(
        usb_iocp_result_t const& result, uintptr_t arg)
{
    pending_cmd_t *cmd = reinterpret_cast<pending_cmd_t*>(arg);
    cmd->if_->usb_completion(cmd);
}

void usb_msc_if_t::usb_completion(pending_cmd_t *cmd)
{
    if (!advance_cmd(cmd))
        return;

    // Entry is finished

    unique_lock<ticketlock> hold_cmd_lock(cmd_lock);
    cmd_tail = cmd_wrap(cmd_tail + 1);
    cmd_cond.notify_one();
}

bool usb_msc_if_t::advance_cmd(pending_cmd_t *cmd)
{
    if (cmd->count == 0)
        return true;

    switch (cmd->phase) {
    case pending_cmd_t::phase_t::tx_cbw:
        cmd->packet.cbw.sig = 0x43425355;
        cmd->packet.cbw.tag = cmd->tag;
        cmd->packet.cbw.xfer_len = cmd->count << cmd->log2_sector_sz;

        if (cmd->op == usb_msc_op_t::read)
            cmd->packet.cbw.flags = 0x80;
        else
            cmd->packet.cbw.flags = 0;

        cmd->packet.cbw.lun = cmd->lun;

        cmd->packet.cbw.wcb_len = sizeof(cmd_rw12_t);
        cmd->packet.cbw.cmd.rw12.op = cmd_read_12;
        cmd->packet.cbw.cmd.rw12.flags = 0;
        cmd->packet.cbw.cmd.rw12.lba = bswap_32(cmd->lba);
        cmd->packet.cbw.cmd.rw12.len = bswap_32(cmd->count);
        cmd->packet.cbw.cmd.rw12.group = 0;
        cmd->packet.cbw.cmd.rw12.control = 0;

        cmd->io_iocp.reset(&usb_msc_if_t::usb_completion);
        cmd->io_iocp.set_expect(1);
        bulk_out.send_async(&cmd->packet, sizeof(cmd->packet), &cmd->io_iocp);
        cmd->phase = pending_cmd_t::phase_t::xfer;
        break;

    case pending_cmd_t::phase_t::xfer:
        cmd->io_iocp.reset(&usb_msc_if_t::usb_completion);
        cmd->io_iocp.set_expect(1);

        if (cmd->op == usb_msc_op_t::read) {
            bulk_in.recv_async(cmd->data, cmd->count << cmd->log2_sector_sz,
                               &cmd->io_iocp);
        } else {
            bulk_out.send_async(cmd->data, cmd->count << cmd->log2_sector_sz,
                                &cmd->io_iocp);
        }

        cmd->phase = pending_cmd_t::phase_t::rx_sbw;
        break;

    case pending_cmd_t::phase_t::rx_sbw:
        cmd->io_iocp.reset(&usb_msc_if_t::usb_completion);
        cmd->io_iocp.set_expect(1);

        bulk_in.recv_async(&cmd->packet.csw, sizeof(cmd->packet.csw),
                           &cmd->io_iocp);

        cmd->phase = pending_cmd_t::phase_t::check;
        break;

    case pending_cmd_t::phase_t::check:
        if (cmd->packet.csw.status != cmd_status_t::success)
            cmd->caller_iocp->set_result(errno_t::EIO);
        else
            cmd->caller_iocp->set_result(errno_t::OK);

        cmd->caller_iocp->invoke();

        return true;
    }

    return false;
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
STORAGE_REGISTER_FACTORY(usb_msc_if);
