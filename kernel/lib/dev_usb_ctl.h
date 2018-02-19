#pragma once
#include "types.h"
#include "device/iocp.h"

class usb_bus_t;

class usb_pipe_t {
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
    virtual bool setup_trb(uint8_t request_type, uint8_t request,
                           uint16_t value, uint16_t index, uint16_t length,
                           void *data) = 0;

    // Returns slot number on success, or negated completion code on error
    virtual int enable_slot(int port) = 0;

    // Returns 0 on success, or negated completion code on error
    virtual int set_address(int port, int slotid) = 0;

private:

};
