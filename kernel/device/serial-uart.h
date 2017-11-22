#pragma once
#include "types.h"
#include "cpu/ioport.h"

class uart_dev_t {
public:
    virtual ~uart_dev_t() {}

    virtual void init(ioport_t port, uint8_t port_irq, uint32_t baud) = 0;

    // Transfer up to size bytes, blocking until at least min is transferred
    // Return number of bytes transferred or negative value for error
    virtual ssize_t write(void const *buf, size_t size, size_t min_write) = 0;
    virtual ssize_t read(void *buf, size_t size, size_t min_read) = 0;

    virtual void route_irq(int) {}

    static uart_dev_t *open(size_t id, bool simple);
};
