#pragma once
#include "types.h"
#include "device/iocp.h"
#include "device/usb.h"

class usb_bus_t;

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

    int send_default_control(uint8_t request_type, uint8_t request,
                             uint16_t value, uint16_t index,
                             uint16_t length, void *data);

    int recv(uint16_t length, void *data);
    int send(uint16_t length, void const *data);

private:
    usb_bus_t *bus;
    int slotid;
    int epid;
};

// Interface to a host controller
class usb_bus_t {
public:
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

    virtual int send_control(
            uint8_t slotid, uint8_t request_type, uint8_t request,
            uint16_t value, uint16_t index, uint16_t length, void *data) = 0;

    virtual int xfer(uint8_t slotid, uint8_t epid, uint16_t stream_id,
                     uint16_t length, void *data, int dir) = 0;

private:

};

class usb_class_drv_t {
public:
    static void find_driver(usb_config_helper *cfg, usb_bus_t *bus);

    usb_class_drv_t();
    virtual bool probe(usb_config_helper *cfg, usb_bus_t *bus) = 0;

protected:
    struct match_result {
        usb_desc_device const* dev;
        usb_desc_config const* cfg;
        usb_desc_iface const* iface;
        int cfg_idx;
        int iface_idx;
    };

    static match_result match_config(
            usb_config_helper *cfg_hlp, int index,
            int dev_class, int dev_subclass,
            int vendor_id, int product_id);

private:
    static usb_class_drv_t *first_driver;
    usb_class_drv_t *next_driver;
};
