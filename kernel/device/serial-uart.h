#pragma once

#include "types.h"
#include "cpu/ioport.h"
#include "cxxstring.h"
#include "chrono.h"
#include "dev_char.h"

class uart_dev_t /*: public dev_char_t*/ {
public:
    using timeout_t = ext::chrono::steady_clock::time_point;

    static uint16_t const com[];
    static uint8_t const irq[];

    uart_dev_t() = default;
    uart_dev_t(uart_dev_t const&) = delete;
    uart_dev_t(uart_dev_t&&) = delete;
    virtual ~uart_dev_t() = 0;

    struct port_cfg_t {
        ioport_t port;
        uint8_t port_irq;
        uint32_t baud;
        uint8_t data_bits;
        char parity_type;
        uint8_t stop_bits;
    };

    virtual bool init(port_cfg_t const& cfg, bool use_irq) = 0;

    bool init(ioport_t port, uint8_t port_irq, uint32_t baud,
              uint8_t data_bits, char parity_type,
              uint8_t stop_bits, bool use_irq);

    using clock = ext::chrono::steady_clock;

    // Transfer up to size bytes, blocking until at least min is transferred
    // Return number of bytes transferred or negative value for error
    virtual ssize_t write(
            void const *buf, size_t size, size_t min_write,
            clock::time_point timeout = clock::time_point::max()) = 0;
    virtual ssize_t read(
            void *buf, size_t size, size_t min_read,
            clock::time_point timeout = clock::time_point::max()) = 0;
    virtual bool wait_dsr_until(timeout_t timeout) = 0;

    // Success because there is no IRQ to route
    virtual bool route_irq(int) { return true; }

    ssize_t write(char c)
    {
        return write(&c, 1);
    }

    template<size_t N>
    _always_inline
    ssize_t wrstr(char const (&str)[N])
    {
        return write((void const *)str, N - 1);
    }

    ssize_t write(ext::string const& str)
    {
        return write(str.data(), str.length());
    }

    ssize_t write(void const *buf, size_t size) //override
    {
        return write(buf, size, size);
    }

    ssize_t read(void *buf, size_t size) //override
    {
        return read(buf, size, size);
    }

    // Simple early UART that does not allocate memory
    static uart_dev_t *open(size_t id, bool simple,
                            uint8_t data_bits, char parity_type,
                            uint8_t stop_bits);

    // Requires memory allocation
    static uart_dev_t *open(uint16_t port, uint8_t irq,
                            uint32_t baud, uint8_t data_bits, char parity_type,
                            uint8_t stop_bits, bool polled);
};
