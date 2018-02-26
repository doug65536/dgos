#include "usb_storage.h"
#include "dev_storage.h"
#include "dev_usb_ctl.h"
#include "bswap.h"
#include "hash_table.h"

class usb_storage_if_factory_t;
class usb_storage_if_t;
class usb_storage_dev_t;
class usb_mass_storage_t;

enum mass_storage_request_t : uint8_t {
    GET_MAX_LUN = 0xFE,
    RESET = 0xFF
};

// A factory that enumerates all of the available storage devices
class usb_storage_if_factory_t : public storage_if_factory_t {
public:
protected:
    // storage_if_factory_t interface
    if_list_t detect() override;
};

// A USB storage interface
class usb_storage_if_t : public storage_if_base_t {
    // storage_if_base_t interface
public:
    STORAGE_IF_IMPL
};

// A LUN on a storage interface
class usb_storage_dev_t : public storage_dev_base_t {
public:
    usb_storage_dev_t();
    ~usb_storage_dev_t();

protected:
    // storage_dev_base_t interface
    STORAGE_DEV_IMPL

    enum struct usb_storage_op_t {
        write,
        read,
        flush,
        trim
    };

    errno_t io(void *data, uint64_t count,
               uint64_t lba, bool fua,
               usb_storage_op_t op, iocp_t *iocp);

    int flush();
    int trim();

    uint8_t log2_sectorsize;
};

// USB mass storage class driver
class usb_mass_storage_t : public usb_class_drv_t {
public:

protected:
    // usb_class_drv_t interface
    bool probe(usb_config_helper *cfg_hlp, usb_bus_t *bus) override final;

private:

    bool reset();
    int get_max_lun();

    enum cmd_op : uint8_t {
        cmd_read_capacity_10 = 0x25,
        cmd_read_12 = 0xA8,
        cmd_write_12 = 0xAA,
        cmd_read_16 = 0x88,
        cmd_write_16 = 0x8A
    };

    // read capacity 10
    struct cmd_rdcap_10_t {
        cmd_op op;  // 0x25
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
        cmd_op op;     // read=0x88, write=0x8A
        uint8_t flags;  // bits: 1=fua_nv, 3=fua, 4=dpo, 7:5 protect
        uint64_t lba;
        uint32_t len;
        uint8_t group;
        uint8_t control;
    } __packed;

    C_ASSERT(sizeof(cmd_rw16_t) == 16);

    struct cmd_rw12_t {
        cmd_op op;     // read=0xA8, write=0xAA
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

    struct pending_cmd_t {
        uint32_t tag;
        iocp_t *iocp;
    };

    hashtbl_t pending_cmds;

    usb_pipe_t control;
    usb_pipe_t bulk_in;
    usb_pipe_t bulk_out;
    uint8_t iface_idx;
};

#define USB_STORAGE_MAX_DEVICES    16
static usb_storage_if_t usb_storage_devices[USB_STORAGE_MAX_DEVICES];
static unsigned usb_storage_count;

#define USB_STORAGE_MAX_DRIVES    64
static usb_storage_dev_t usb_storage_drives[USB_STORAGE_MAX_DRIVES];
static unsigned usb_storage_drive_count;

//
// Storage interface factory

if_list_t usb_storage_if_factory_t::detect()
{
    unsigned start_at = usb_storage_count;
    if_list_t list = {
        usb_storage_devices + start_at,
        sizeof(*usb_storage_devices),
        0
    };

    return list;
}

//
// Storage interface

void usb_storage_if_t::cleanup()
{
}

if_list_t usb_storage_if_t::detect_devices()
{
    unsigned start_at = usb_storage_drive_count;
    if_list_t list = {
        usb_storage_drives + start_at,
        sizeof(*usb_storage_drives),
        0
    };

    return list;
}

//
// USB device (LUN)

void usb_storage_dev_t::cleanup()
{
}

errno_t usb_storage_dev_t::read_async(
        void *data, int64_t count,
        uint64_t lba, iocp_t *iocp)
{
    //USB_STORAGE_TRACE("Reading %ld blocks at LBA %lx", count, lba);

    return io(data, count, lba, false, usb_storage_op_t::read, iocp);
}

errno_t usb_storage_dev_t::write_async(
        void const *data, int64_t count,
        uint64_t lba, bool fua, iocp_t *iocp)
{
    return io((void*)data, count, lba, fua, usb_storage_op_t::write, iocp);
}

errno_t usb_storage_dev_t::flush_async(iocp_t *iocp)
{
    return io(nullptr, 0, 0, false, usb_storage_op_t::flush, iocp);
}

errno_t usb_storage_dev_t::trim_async(int64_t count,
        uint64_t lba, iocp_t *iocp)
{
    return io(nullptr, count, lba, false, usb_storage_op_t::trim, iocp);
}

long usb_storage_dev_t::info(storage_dev_info_t key)
{
    switch (key) {
    case STORAGE_INFO_BLOCKSIZE:
        return 1L << log2_sectorsize;

    case STORAGE_INFO_HAVE_TRIM:
        return 1;

    default:
        return 0;
    }
}

//
// USB mass storage class driver

bool usb_mass_storage_t::probe(usb_config_helper *cfg_hlp, usb_bus_t *bus)
{
    // Match SCSI mass storage devices
    match_result match = match_config(
                cfg_hlp, 0, int(usb_class_t::mass_storage), -1, -1, -1);

    if (!match.dev)
        return false;

    if (!bus->get_pipe(cfg_hlp->slot(), 0, control))
        return false;

    usb_desc_ep const *ep = nullptr;

    for (int i = 0; (ep = cfg_hlp->find_ep(match.iface, i)) != nullptr; ++i) {
        usb_pipe_t &pipe = ep->ep_addr >= 0x80 ? bulk_in : bulk_out;
        if (!bus->alloc_pipe(cfg_hlp->slot(), ep->ep_addr, pipe,
                             ep->max_packet_sz, ep->interval, ep->ep_attr))
            return false;
    }

    // Initialize pending command table
    htbl_create(&pending_cmds, offsetof(pending_cmd_t, tag),
                sizeof(pending_cmd_t::tag));

    // hack
    reset();

    int max_lun = get_max_lun();

    rdcap_10_data_t capacity{};

    cbw_t cbw{};
    cbw.sig = 0x43425355;
    cbw.tag = 42;
    cbw.xfer_len = 512;
    cbw.flags = 0x80;
    cbw.lun = 0;
    cbw.wcb_len = sizeof(cbw.cmd.rw12);
    cbw.cmd.rw12 = {};
    cbw.cmd.rw12.op = cmd_read_12;
    cbw.cmd.rw12.flags = 0;
    cbw.cmd.rw12.lba = 0;
    cbw.cmd.rw12.len = bswap_32(1);
    cbw.cmd.rw12.group = 0;
    cbw.cmd.rw12.control = 0;

    int cbw_cc = bulk_out.send(&cbw, sizeof(cbw));

    char buf[512];

    int recv_cc = bulk_in.recv(buf, sizeof(buf));

    csw_t csw{};

    int sbw_cc = bulk_in.recv(&csw, sizeof(csw));

    return true;
}

bool usb_mass_storage_t::reset()
{
    if (control.send_default_control(
                uint8_t(usb_dir_t::OUT) |
                (uint8_t(usb_req_type::CLASS) << 5) |
                uint8_t(usb_req_recip_t::INTERFACE),
                uint8_t(mass_storage_request_t::RESET),
                0, iface_idx, 0, nullptr) < 0)
        return false;

    // FIXME: Clear_Feature(HALT) both the IN and OUT pipes

    return true;
}

int usb_mass_storage_t::get_max_lun()
{
    uint8_t max_lun;

     int ncc = control.send_default_control(
                uint8_t(usb_dir_t::IN) |
                (uint8_t(usb_req_type::CLASS) << 5) |
                uint8_t(usb_req_recip_t::INTERFACE),
                uint8_t(mass_storage_request_t::GET_MAX_LUN),
                0, iface_idx, sizeof(max_lun), &max_lun);

     if (ncc == -int(usb_cc_t::stall_err)) {
         // Not supported, assume 1
         return 1;
     }

     return ncc >= 0 ? max_lun : ncc;
}

usb_storage_dev_t::usb_storage_dev_t()
{

}

usb_storage_dev_t::~usb_storage_dev_t()
{

}

errno_t usb_storage_dev_t::io(void *data, uint64_t count, uint64_t lba,
                              bool fua, usb_storage_dev_t::usb_storage_op_t op,
                              iocp_t *iocp)
{
    return errno_t::ENOSYS;
}

int usb_storage_dev_t::flush()
{
    return -int(errno_t::ENOSYS);
}

int usb_storage_dev_t::trim()
{
    return -int(errno_t::ENOSYS);
}

static usb_mass_storage_t usb_mass_storage;
