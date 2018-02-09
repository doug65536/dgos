#pragma once

// Interface to a host controller
class usb_bus_t {
public:
    // Reset the host controller
    virtual bool reset() = 0;

    // Get the number of ports
    virtual int port_count() = 0;

    // Get port current connect status (true = device connected)
    virtual bool port_connect_status(int port) = 0;

    // Allocate resources for a port
    virtual bool enable_slot(int port) = 0;

private:

};
