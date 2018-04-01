#pragma once
#include "types.h"
#include "device/iocp.h"
#include "device/usb.h"

class usb_bus_t;

struct usb_iocp_result_t {
    usb_iocp_result_t()
        : ccp(0)
        , xfer_len(0)
        , cc(usb_cc_t::invalid)
        , slotid(0)
    {
    }

    operator bool() const
    {
        return cc == usb_cc_t::success;
    }

    int len_or_error()
    {
        return cc == usb_cc_t::success || cc == usb_cc_t::short_pkt
                ? xfer_len
                : -int(cc);
    }

    int slot_or_error()
    {
        return cc == usb_cc_t::success ? slotid : -int(cc);
    }

    uint32_t ccp;
    uint32_t xfer_len;
    usb_cc_t cc;
    uint8_t slotid;

    static bool succeeded(usb_iocp_result_t const& status)
    {
        return status.cc == usb_cc_t::success;
    }
};

using usb_iocp_t = basic_iocp_t<usb_iocp_result_t>;
using usb_blocking_iocp_t = basic_blocking_iocp_t<usb_iocp_result_t>;

class usb_pipe_t {
public:
    usb_pipe_t()
        : bus(nullptr)
        , slotid(-1)
        , epid(-1)
    {
    }

    usb_pipe_t(usb_bus_t *bus, int slotid, int epid)
        : bus(bus)
        , slotid(slotid)
        , epid(epid)
    {
    }

    operator bool() const
    {
        return bus && slotid > 0 && epid >= 0;
    }

    int send_default_control(uint8_t request_type, uint8_t request,
                             uint16_t value, uint16_t index,
                             uint16_t length, void *data) const;

    int send_default_control_async(uint8_t request_type, uint8_t request,
                                   uint16_t value, uint16_t index,
                                   uint16_t length, void *data,
                                   usb_iocp_t *iocp) const;

    int recv(void *data, uint32_t length) const;
    int recv_async(void *data, uint32_t length, usb_iocp_t *iocp) const;

    int send(void const *data, uint32_t length) const;
    int send_async(void const *data, uint32_t length, usb_iocp_t *iocp) const;

    int clear_ep_halt(usb_pipe_t const& target);

    bool add_hub_port(int port);

    bool set_hub_port_count(usb_hub_desc const& hub_desc);

private:
    usb_bus_t *bus;
    int slotid;
    int epid;
};

// Interface to a host controller
class usb_bus_t {
public:
    virtual ~usb_bus_t() {}

    // Reset the host controller
    virtual bool reset() { return false; }

    // Get the highest port number supported by this device
    virtual int get_max_ports() = 0;

    // Get port current connect status
    virtual bool port_device_present(int port) = 0;

    // Returns slot number on success, or negated completion code on error
    virtual int enable_slot(int port) = 0;

    // Returns 0 on success, or negated completion code on error
    virtual int set_address(int slotid, int port, uint32_t route) = 0;

    virtual bool get_pipe(int slotid, int epid, usb_pipe_t &pipe) = 0;

    virtual bool alloc_pipe(int slotid, int epid, usb_pipe_t &pipe,
                            int max_packet_sz, int interval,
                            usb_ep_attr ep_type) = 0;

    bool alloc_pipe(int slotid, usb_desc_ep const* ep, usb_pipe_t &pipe);

    virtual int send_control(
            int slotid, uint8_t request_type, uint8_t request,
            uint16_t value, uint16_t index, uint16_t length, void *data) = 0;

    virtual int send_control_async(
            int slotid, uint8_t request_type, uint8_t request,
            uint16_t value, uint16_t index, uint16_t length, void *data,
            usb_iocp_t *iocp) = 0;

    virtual int xfer(int slotid, uint8_t epid, uint16_t stream_id,
                     uint32_t length, void *data, int dir) = 0;

    virtual int xfer_async(
            int slotid, uint8_t epid, uint16_t stream_id,
            uint32_t length, void *data, int dir,
            usb_iocp_t *iocp) = 0;

    virtual usb_ep_state_t get_ep_state(int slotid, uint8_t epid) = 0;

    virtual int reset_ep(int slotid, uint8_t epid) = 0;
    virtual int reset_ep_async(int slotid, uint8_t epid, usb_iocp_t *iocp) = 0;

    virtual bool configure_hub_port(int slotid, int port) = 0;

    virtual bool set_hub_port_count(int slotid,
                                    usb_hub_desc const& hub_desc) = 0;

private:

};

class usb_class_drv_t {
public:
    static void find_driver(usb_config_helper *cfg, usb_bus_t *bus);

    usb_class_drv_t();
    virtual bool probe(usb_config_helper *cfg_hlp, usb_bus_t *bus) = 0;

protected:
    struct match_result {
        usb_desc_device const* dev;
        usb_desc_config const* cfg;
        usb_desc_iface const* iface;
        int cfg_idx;
        int iface_idx;
    };

    static match_result match_config(usb_config_helper *cfg_hlp, int index,
            int dev_class, int dev_subclass, int dev_proto,
            int vendor_id, int product_id);

private:
    static usb_class_drv_t *first_driver;
    usb_class_drv_t *next_driver;
};
