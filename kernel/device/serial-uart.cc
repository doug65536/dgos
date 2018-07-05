#include "serial-uart.h"
#include "cpu/ioport.h"
#include "callout.h"
#include "vector.h"
#include "irq.h"
#include "mutex.h"
#include "cpu/control_regs.h"
#include "nano_time.h"

#define DEBUG_UART  0
#if DEBUG_UART
#define UART_TRACE(...) printdbg("uart: " __VA_ARGS__)
#else
#define UART_TRACE(...) ((void)0)
#endif

// Null modem connections:
//
//  Peer      Peer
//   TxD  ->   RxD
//   DTR  ->   DSR
//   RTS  ->   CTS
//   RxD  <-   TxD
//   DSR  <-   DTR
//

namespace uart_defs {

enum struct special_val_t : uint16_t {
    // Tx/Rx break
    BREAK = 256,
    FRAMING_ERR,
    PARITY_ERR
};

enum struct port_t : uint16_t {
    // Send/Receive data (read/write)
    DAT = 0,

    // Baud rate lo
    DLL = 0,

    // Interrupt enable register
    IER = 1,

    // Baud rate hi
    DLM = 1,

    // Interrupt identification register (read only)
    IIR = 2,

    // FIFO control register (write only)
    FCR = 2,

    // Line control register
    LCR = 3,

    // Modem control register
    MCR = 4,

    // Line status register
    LSR = 5,

    // Modem status register
    MSR = 6,

    // Scratch register
    SCR = 7
};

struct dat_t {
    uint8_t value;
};

union ier_t {
    struct bits_t {
        // Byte available in rx register
        bool rx_avail:1;

        // Tx FIFO empty, may write 16 bytes
        bool tx_empty:1;

        // Enable IRQ on error
        bool error:1;

        // Enable IRQ on status change
        bool status:1;

        uint8_t unused:4;
    } bits;
    uint8_t value;
};

enum struct iir_source_t {
    STATUS_CHG = 0,
    TX_EMPTY = 1,
    RX_AVAIL = 2,
    ERROR = 3
};

union iir_t {
    struct bits_t {
        // 0=IRQ occurred
        uint8_t nintr:1;

        // Source of IRQ
        uint8_t source:2;

        uint8_t unused:5;
    } bits;
    uint8_t value;
};

enum struct fcr_rx_trigger_t {
    FIFO1 = 0,
    FIFO4 = 1,
    FIFO8 = 2,
    FIFO14 = 3
};

union fcr_t {
    struct bits_t {
        // Enable tx/rx FIFOs
        bool fifo_en:1;

        // Self-clearing tx/rx FIFO resets
        bool fifo_rx_reset:1;
        bool fifo_tx_reset:1;

        bool dma_mode:1;

        bool unused:1;

        // 16750 only
        bool fifo64:1;

        // Rx FIFO trigger threshold
        uint8_t rx_trigger:2;
    } bits;
    uint8_t value;
};

enum struct lcr_wordlen_t : uint8_t {
    DATA5 = 0,
    DATA6 = 1,
    DATA7 = 2,
    DATA8 = 3
};

enum struct lcr_stopbits_t : uint8_t {
    STOP1 = 0,
    STOP2 = 1
};

enum struct lcr_parity_t : uint8_t {
    ODD = 0,
    EVEN = 1,
    MARK = 2,
    SPACE = 3
};

union lcr_t {
    struct bits_t {
        // Configure number of data bits per character
        uint8_t wordlen:2;

        // Configure number of stop bits per character
        uint8_t stopbits:1;

        // Enable parity
        bool parity_en:1;

        // Configure parity method
        uint8_t parity:2;

        // Transmit a break signal
        bool tx_break:1;

        // Toggle between tx/rx/ier and dll/dlm registers in register 0 and 1
        bool baud_latch:1;
    } bits;
    uint8_t value;
};

union mcr_t {
    struct bits_t {
        // Tell the peer we are ready for a communication channel
        bool dtr:1;

        // Tell the peer we are ready to rx
        bool rts:1;

        // Enable IRQs (externally ANDed with int_en to IRQ line)
        bool out1:1;

        // Enable IRQs
        bool int_en:1;

        // Enable loopback mode (does not generate IRQs)
        bool loopback:1;

        uint8_t unused:3;
    } bits;
    uint8_t value;
};

union lsr_t {
    struct bits_t {
        bool rx_data:1;
        bool overrun:1;
        bool parity_err:1;
        bool framing_err:1;

        // Break in progress
        bool break_intr:1;

        // Tx data register empty
        bool tx_hold_empty:1;

        // Tx shift register empty
        bool tx_sr_empty:1;

        // Parity error, framing error, or break, in FIFO somewhere
        bool err_in_fifo:1;
    } bits;
    uint8_t value;
};

union msr_t {
    struct bits_t {
        // CTS input changed
        bool cts_change:1;

        // DSR input changed
        bool dsr_change:1;

        // RI input changed
        bool ring_done:1;

        // DCR input changed
        bool dcr_change:1;

        // Peer is ready to receive more data, allowed to tx
        bool cts:1;

        // Peer is ready for a communication channel
        bool dsr:1;

        // RI input state
        bool ring:1;

        // DCD input state
        bool dcd:1;
    } bits;
    uint8_t value;
};

union scr_t {
    uint8_t value;
};

enum struct chiptype_t : uint8_t {
    UNKNOWN,
    U8250,
    U16450,
    U16550,
    U16550A,
    U16750
};

template<typename> struct reg_t;

template<> struct reg_t<dat_t> {
    static constexpr port_t ofs = port_t::DAT;
};

template<> struct reg_t<ier_t> {
    static constexpr port_t ofs = port_t::IER;
};

template<> struct reg_t<iir_t> {
    static constexpr port_t ofs = port_t::IIR;
};

template<> struct reg_t<fcr_t> {
    static constexpr port_t ofs = port_t::FCR;
};

template<> struct reg_t<lcr_t> {
    static constexpr port_t ofs = port_t::LCR;
};

template<> struct reg_t<mcr_t> {
    static constexpr port_t ofs = port_t::MCR;
};

template<> struct reg_t<lsr_t> {
    static constexpr port_t ofs = port_t::LSR;
};

template<> struct reg_t<msr_t> {
    static constexpr port_t ofs = port_t::MSR;
};

template<> struct reg_t<scr_t> {
    static constexpr port_t ofs = port_t::SCR;
};

class uart_t : public uart_dev_t
{
public:
    // Configure the UART specified by port and IRQ using 8N1
    uart_t();
    virtual ~uart_t() override;

protected:
    bool init(ioport_t port, uint8_t port_irq, uint32_t baud,
              uint8_t data_bits, char parity_type, uint8_t stop_bits,
              bool use_irq);

    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);

    virtual isr_context_t *port_irq_handler(isr_context_t *ctx);

    _always_inline void out(port_t ofs, uint8_t value) const;
    _always_inline void outs(port_t ofs, const void *value, size_t len) const;

    template<typename T>
    _always_inline void outp(T const& reg)
    {
        out(reg_t<T>::ofs, reg.value);
    }

    _always_inline uint8_t in(port_t ofs)
    {
        return inb(io_base + ioport_t(ofs));
    }

    template<typename T>
    _always_inline T inp(T& reg)
    {
        reg.value = in(reg_t<T>::ofs);
        return reg;
    }

    _const
    _always_inline uint16_t queue_next(
            uint16_t value, uint8_t log2_buffer_size) const
    {
        return (value + 1) & ~-(1 << log2_buffer_size);
    }

    // Updates shadow register only, no I/O done
    bool set_framing(uint8_t data, char parity, uint8_t stop)
    {
        lcr_wordlen_t reg_wordlen;
        lcr_parity_t reg_parity;
        bool reg_parity_en;
        lcr_stopbits_t reg_stop;

        switch (data) {
        case 5: reg_wordlen = lcr_wordlen_t::DATA5; break;
        case 6: reg_wordlen = lcr_wordlen_t::DATA6; break;
        case 7: reg_wordlen = lcr_wordlen_t::DATA7; break;
        case 8: reg_wordlen = lcr_wordlen_t::DATA8; break;
        default: return false;
        }

        switch (parity) {
        case 'N': reg_parity = lcr_parity_t::SPACE; break;
        case 'O': reg_parity = lcr_parity_t::ODD; break;
        case 'E': reg_parity = lcr_parity_t::EVEN; break;
        case 'M': reg_parity = lcr_parity_t::MARK; break;
        case 'S': reg_parity = lcr_parity_t::SPACE; break;
        default: return false;
        }

        reg_parity_en = (parity != 'N');

        switch (stop) {
        case 1: reg_stop = lcr_stopbits_t::STOP1; break;
        case 2: reg_stop = lcr_stopbits_t::STOP2; break;
        default: return false;
        }

        reg_lcr.bits.wordlen = uint8_t(reg_wordlen);
        reg_lcr.bits.stopbits = uint8_t(reg_stop);
        reg_lcr.bits.parity_en = reg_parity_en;
        reg_lcr.bits.parity = uint8_t(reg_parity);

        return true;
    }

    // Shadow registers
    ier_t reg_ier;
    iir_t reg_iir;
    fcr_t reg_fcr;
    lcr_t reg_lcr;
    mcr_t reg_mcr;
    lsr_t reg_lsr;
    msr_t reg_msr;
    scr_t reg_scr;

    uint16_t rx_head;
    uint16_t rx_tail;

    ioport_t io_base;
    uint8_t irq;
    bool irq_hooked;

    chiptype_t chip;
    uint8_t fifo_size;
    uint32_t max_baud;
};

static vector<unique_ptr<uart_t>> uarts;

uart_t::uart_t()
    : rx_head(0)
    , rx_tail(0)
    , io_base(0)
    , irq(0)
    , irq_hooked(false)
    , chip(chiptype_t::UNKNOWN)
    , fifo_size(0)
    , max_baud(0)
{
    reg_ier.value = 0;
    reg_iir.value = 0;
    reg_fcr.value = 0;
    reg_lcr.value = 0;
    reg_mcr.value = 0;
    reg_lsr.value = 0;
    reg_msr.value = 0;
    reg_scr.value = 0;
}

bool uart_t::init(ioport_t port, uint8_t port_irq, uint32_t baud,
                  uint8_t data_bits, char parity_type, uint8_t stop_bits,
                  bool use_irq)
{
    io_base = port;
    irq = port_irq;

    // Initialize shadow registers
    // (and acknowledge any pending interrupts too)
    inp(reg_ier);
    inp(reg_iir);
    inp(reg_lcr);
    inp(reg_mcr);
    inp(reg_lsr);
    inp(reg_msr);
    inp(reg_scr);
    in(port_t::DAT);

    // Detect type of UART
    reg_fcr.value = 0;
    reg_fcr.bits.fifo_en = 1;
    reg_fcr.bits.fifo_rx_reset = 1;
    reg_fcr.bits.fifo_tx_reset = 1;
    reg_fcr.bits.fifo64 = 1;
    reg_fcr.bits.rx_trigger = 3;
    outp(reg_fcr);

    inp(reg_fcr);
    if (reg_fcr.bits.rx_trigger == 3 && reg_fcr.bits.fifo64) {
        // 64 byte fifo bit was writable, it is a 16750
        chip = chiptype_t::U16750;
        fifo_size = 64;
        max_baud = 115200;
    } else if (reg_fcr.bits.rx_trigger == 3 && !reg_fcr.bits.fifo64) {
        // Both rx_trigger bits were writable, it is a good 16550A
        // Good 16550A
        chip = chiptype_t::U16550A;
        fifo_size = 16;
        max_baud = 115200;
    } else if (reg_fcr.bits.rx_trigger == 1) {
        // Bit 6 was not preserved, is a crap 16550
        // Buggy 16550
        chip = chiptype_t::U16550;
        fifo_size = 1;
        max_baud = 57600;
    } else {
        // If the scratch register preserves value, it is a 16450
        reg_scr.value = 0x55;
        outp(reg_scr);
        inp(reg_scr);

        if (reg_scr.value == 0x55) {
            chip = chiptype_t::U16450;
            max_baud = 38400;
        } else {
            chip = chiptype_t::U8250;
            max_baud = 19200;
        }

        fifo_size = 1;
    }

    // Enable access to baud rate registers
    reg_lcr.bits.baud_latch = 1;

    // Might as well program the rest of LCR here
    set_framing(data_bits, parity_type, stop_bits);
    reg_lcr.bits.tx_break = 0;

    outp(reg_lcr);

    // Set baud rate
    uint16_t divisor = 115200 / baud;
    out(port_t::DLL, (divisor >> (0*8)) & 0xFF);
    out(port_t::DLM, (divisor >> (1*8)) & 0xFF);

    // Disable access to baud rate registers
    reg_lcr.bits.baud_latch = 0;
    outp(reg_lcr);

    //
    // Configure and reset FIFO

    if (fifo_size > 1) {
        reg_fcr.value = 0;
        reg_fcr.bits.fifo_en = 1;
        reg_fcr.bits.fifo_rx_reset = 1;
        reg_fcr.bits.fifo_tx_reset = 1;
        reg_fcr.bits.rx_trigger = uint8_t(fcr_rx_trigger_t::FIFO8);
        outp(reg_fcr);
    }

    //
    // Unmask interrupts

    reg_ier.value = 0;
    reg_ier.bits.error = use_irq;
    reg_ier.bits.status = use_irq;
    reg_ier.bits.rx_avail = use_irq;
    reg_ier.bits.tx_empty = use_irq;
    outp(reg_ier);

    //
    // Configure modem control outputs

    reg_mcr.value = 0;

    // Assert DTR, port is now open
    reg_mcr.bits.dtr = 1;

    // Disable IRQ
    reg_mcr.bits.out1 = 0;
    reg_mcr.bits.int_en = 0;

    // Don't assert RTS immediately when polling
    reg_mcr.bits.rts = use_irq;

    outp(reg_mcr);

    // Acknowledge everything one more time, just in case
    inp(reg_ier);
    inp(reg_iir);
    inp(reg_lcr);
    inp(reg_mcr);
    inp(reg_lsr);
    inp(reg_msr);
    inp(reg_scr);
    in(port_t::DAT);

    if (use_irq) {
        irq_hook(irq, &uart_t::irq_handler, "serial_uart");
        irq_setmask(irq, true);
        irq_hooked = true;

        // Enable IRQ if requested
        reg_mcr.bits.out1 = use_irq;
        reg_mcr.bits.int_en = use_irq;
        outp(reg_mcr);
    }

    return true;
}

uart_t::~uart_t()
{
    reg_mcr.bits.int_en = 0;
    outp(reg_mcr);
}

isr_context_t *uart_t::irq_handler(int irq, isr_context_t *ctx)
{
    //printdbg("UART IRQ\n");

    for (uart_t *uart : uarts) {
        if (uart->irq_hooked && uart->irq == irq)
            ctx = uart->port_irq_handler(ctx);
    }
    return ctx;
}

isr_context_t *uart_t::port_irq_handler(isr_context_t *ctx)
{
    return ctx;
}

void uart_t::out(port_t ofs, uint8_t value) const
{
    outb(io_base + ioport_t(ofs), value);
}

void uart_t::outs(port_t ofs, void const *value, size_t len) const
{
    if (len > 1)
        outsb(io_base + ioport_t(ofs), value, len);
    else if (len == 1)
        outb(io_base + ioport_t(ofs), ((uint8_t*)value)[0]);
}

// ===========================================================================
// IRQ driven, asynchronous UART implementation

class uart_async_t : public uart_t {
public:
    uart_async_t();
    ~uart_async_t();

    bool init(ioport_t port, uint8_t port_irq, uint32_t baud,
              uint8_t data_bits, char parity_type,
              uint8_t stop_bits) override final;

    ssize_t write(void const *buf, size_t size,
                  size_t min_write) override final;
    ssize_t read(void *buf, size_t size, size_t min_read) override final;

    void route_irq(int cpu) override final;

private:
    using lock_type = mcslock;
    using scoped_lock = unique_lock<lock_type>;

    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    isr_context_t *port_irq_handler(isr_context_t *ctx) override final;

    void wait_tx_not_full(scoped_lock &lock_);
    void wait_rx_not_empty(scoped_lock& lock_);

    void rx_enqueue(uint16_t value);

    void send_some(scoped_lock const&);

    bool is_rx_full() const;
    bool is_tx_full() const;

    _always_inline bool is_tx_empty() const
    {
        return tx_head == tx_tail;
    }

    _always_inline bool is_rx_empty() const
    {
        return rx_head == rx_tail;
    }

    uint16_t tx_peek_value() const;
    void tx_take_value();

    condition_variable tx_not_full;
    condition_variable rx_not_empty;

    // Use 16 bit values to allow buffering of error information
    // as values >= 256
    unique_ptr<uint16_t> rx_buffer;
    unique_ptr<uint16_t> tx_buffer;

    lock_type lock;

    uint16_t tx_head;
    uint16_t tx_tail;

    uint8_t log2_buffer_size;

    bool sending_break;
    bool sending_data;
};

uart_async_t::uart_async_t()
    : uart_t()
    , tx_head(0)
    , tx_tail(0)
    , log2_buffer_size(16)
    , sending_break(false)
    , sending_data(false)
{
    rx_buffer = new uint16_t[1 << log2_buffer_size];
    tx_buffer = new uint16_t[1 << log2_buffer_size];
}

uart_async_t::~uart_async_t()
{
    if (irq_hooked) {
        irq_setmask(irq, false);
        irq_unhook(irq, &uart_t::irq_handler);
        irq_hooked = false;
    }
}

bool uart_async_t::init(ioport_t port, uint8_t port_irq, uint32_t baud,
                        uint8_t data_bits, char parity_type, uint8_t stop_bits)
{
    return uart_t::init(port, port_irq, baud,
                        data_bits, parity_type, stop_bits, true);
}

ssize_t uart_async_t::write(void const *buf, size_t size, size_t min_write)
{
    scoped_lock lock_(lock);

    auto data = (char const *)buf;

    size_t i;
    for (i = 0; i < size; ++i) {
        if (is_tx_full()) {
            if (i >= min_write)
                break;

            do {
                if (!sending_data)
                    send_some(lock_);

                wait_tx_not_full(lock_);
            } while (is_tx_full());
        }

        tx_buffer[tx_head] = *data++;
        tx_head = queue_next(tx_head, log2_buffer_size);
    }

    if (!sending_data)
        send_some(lock_);

    return i;
}

ssize_t uart_async_t::read(void *buf, size_t size, size_t min_read)
{
    scoped_lock lock_(lock);

    auto data = (uint8_t *)buf;

    size_t i;
    for (i = 0; i < size; ++i) {
        if (is_rx_empty()) {
            // Reset buffer to maximize locality
            rx_head = 0;
            rx_tail = 0;

            if (i >= min_read)
                break;

            wait_rx_not_empty(lock_);
        }

        *data++ = rx_buffer[rx_tail];
        rx_tail = queue_next(rx_tail, log2_buffer_size);
    }

    return i;
}

void uart_async_t::route_irq(int cpu)
{
    irq_setcpu(irq, cpu);
}

void uart_async_t::send_some(scoped_lock const&)
{
    sending_data = !is_tx_empty();

    for (size_t i = 0; i < fifo_size && !is_tx_empty(); ++i) {
        // Peek at next value without removing it from the queue
        uint16_t tx_value = tx_peek_value();

        switch (tx_value) {
        default:
            if (unlikely(sending_break)) {
                // Stop sending break
                reg_lcr.bits.tx_break = 0;
                outp(reg_lcr);
                sending_break = false;
            }

            out(port_t::DAT, uint8_t(tx_value));

            // Remove the transmitted character from the send queue
            tx_take_value();

            break;

        case uint8_t(special_val_t::BREAK):
            if (!sending_break) {
                sending_break = true;

                // Start sending break
                reg_lcr.bits.tx_break = 1;
                outp(reg_lcr);
            }

            // Tx a null byte (which will not be sent but will
            // give us a tx empty interrupt after one character
            // time)
            out(port_t::DAT, 0);

            tx_take_value();

            // Don't put more data into Tx FIFO after a break byte
            i = fifo_size;

            break;
        }
    }

    if (tx_head == tx_tail) {
        // Reset buffer pointers to maximize locality
        tx_head = 0;
        tx_tail = 0;
    }
}

void uart_async_t::rx_enqueue(uint16_t value)
{
    rx_buffer[rx_head] = value;
    rx_head = queue_next(rx_head, log2_buffer_size);
}

bool uart_async_t::is_rx_full() const
{
    // Newly received bytes are placed at rx_head
    // Stream reads take bytes from rx_tail
    return queue_next(rx_head, log2_buffer_size) == rx_tail;
}

bool uart_async_t::is_tx_full() const
{
    // Stream writes place bytes at tx_head
    // Transmit takes bytes from tx_tail
    return queue_next(tx_head, log2_buffer_size) == tx_tail;
}

uint16_t uart_async_t::tx_peek_value() const
{
    return tx_buffer[tx_tail];
}

void uart_async_t::tx_take_value()
{
    tx_tail = queue_next(tx_tail, log2_buffer_size);
}

isr_context_t *uart_async_t::port_irq_handler(isr_context_t *ctx)
{
    scoped_lock lock_(lock);

    bool wake_tx = false;
    bool wake_rx = false;

    for (inp(reg_iir); !reg_iir.bits.nintr && !is_rx_full(); inp(reg_iir)) {
        switch (reg_iir.bits.source) {
        case uint8_t(iir_source_t::ERROR):
            // Overrun error, parity error, framing error, break
            inp(reg_lsr);

            if (reg_lsr.bits.break_intr)
                rx_enqueue(uint16_t(special_val_t::BREAK));
            if (reg_lsr.bits.framing_err)
                rx_enqueue(uint16_t(special_val_t::FRAMING_ERR));
            if (reg_lsr.bits.parity_err)
                rx_enqueue(uint16_t(special_val_t::PARITY_ERR));

            wake_rx = true;

            break;

        case uint8_t(iir_source_t::RX_AVAIL):
            rx_enqueue(in(port_t::DAT));
            wake_rx = true;
            break;

        case uint8_t(iir_source_t::TX_EMPTY):
            send_some(lock_);
            wake_tx = true;

            break;

        case uint8_t(iir_source_t::STATUS_CHG):
            inp(reg_msr);
            break;

        }
    }

    if (wake_tx) {
        UART_TRACE("Notifying tx\n");
        tx_not_full.notify_all();
    }

    if (wake_rx) {
        UART_TRACE("Notifying rx\n");
        rx_not_empty.notify_all();
    }

    return ctx;
}

void uart_async_t::wait_tx_not_full(scoped_lock& lock_)
{
    UART_TRACE("Blocking on tx\n");
    tx_not_full.wait(lock_);
    UART_TRACE("Unblocked tx\n");
}

void uart_async_t::wait_rx_not_empty(scoped_lock& lock_)
{
    do {
        UART_TRACE("Blocking rx\n");
        rx_not_empty.wait(lock_);
        UART_TRACE("Unblocked rx\n");
    } while (is_rx_empty());
}


// ===========================================================================
// Polling driven synchronous UART

class uart_poll_t : public uart_t {
public:
    uart_poll_t();

    bool init(ioport_t port, uint8_t port_irq, uint32_t baud,
              uint8_t data_bits, char parity_type,
              uint8_t stop_bits) override final;
    virtual ssize_t write(const void *buf, size_t size,
                          size_t min_write) override final;
    virtual ssize_t read(void *buf, size_t size,
                         size_t min_read) override final;

private:
    bool is_rx_full() const;

    _always_inline bool is_rx_empty() const
    {
        return rx_tail == rx_head;
    }

    uint8_t rx_dequeue();
    void rx_enqueue(uint8_t value);

    static constexpr uint8_t log2_buffer_size = 7;
    uint8_t rx_buffer[1 << log2_buffer_size];
    uint8_t rx_head;
    uint8_t rx_tail;
};

uart_poll_t::uart_poll_t()
    : rx_head(0)
    , rx_tail(0)
{
}

bool uart_poll_t::init(ioport_t port, uint8_t port_irq, uint32_t baud,
                       uint8_t data_bits, char parity_type, uint8_t stop_bits)
{
    return uart_t::init(port, port_irq, baud,
                        data_bits, parity_type, stop_bits, false);
}

ssize_t uart_poll_t::write(const void *buf, size_t size, size_t min_write)
{
    for (size_t i = 0; i < size; ) {
        // Wait for tx holding register to be empty
        for (;;) {
            inp(reg_lsr);

            if (reg_lsr.bits.tx_hold_empty)
                break;

            if (i >= min_write)
                return ssize_t(i);

            pause();
        }

        // Wait for CTS
        for (;;) {
            inp(reg_msr);

            if (reg_msr.bits.cts)
                break;

            if (i >= min_write)
                return ssize_t(i);

            pause();
        }

        // Fill the FIFO or send the remainder
        size_t block_size = min(size_t(fifo_size), size - i);

        outs(port_t::DAT, (char*)buf + i, block_size);
        i += block_size;
    }

    return size;
}

ssize_t uart_poll_t::read(void *buf, size_t size, size_t min_read)
{
    // First drain the buffer
    size_t i = 0;

    uint8_t *rxbuf = (uint8_t*)buf;

    while (i < size && !is_rx_empty())
        rxbuf[i++] = rx_dequeue();

    // +------------+-----------+
    // | Rate (cps) | time/byte |
    // +------------+-----------+
    // |        240 |    4.1 ms |
    // |        960 |    1.0 ms |
    // |       1920 |    520 us |
    // |       3840 |    260 us |
    // |      11520 |     65 us |
    // |          n |  1e6/n us |
    // +------------+-----------+

    while (i < size) {
        inp(reg_lsr);

        if (reg_lsr.bits.rx_data) {
            // Receive incoming byte
            ((uint8_t*)buf)[i++] = in(port_t::DAT);
        } else if (!reg_mcr.bits.rts) {
            // Raise RTS
            reg_mcr.bits.rts = 1;
            outp(reg_mcr);
        } else if (i >= min_read) {
            // We've read enough, don't poll
            break;
        } else {
            pause();
        }
    }

    // Drop RTS when not in read function
    if (reg_mcr.bits.rts) {
        reg_mcr.bits.rts = 0;
        outp(reg_mcr);
    }

    return 0;
}

bool uart_poll_t::is_rx_full() const
{
    return queue_next(rx_head, log2_buffer_size) == rx_tail;
}

uint8_t uart_poll_t::rx_dequeue()
{
    uint8_t value = rx_buffer[rx_tail];
    rx_tail = queue_next(rx_tail, log2_buffer_size);
    return value;
}

static uart_poll_t debug_uart;

}   // namespace


// ===========================================================================
//

uart_dev_t *uart_dev_t::open(size_t id, bool simple, uint8_t data_bits,
                             char parity_type, uint8_t stop_bits)
{
    if (!simple)
        return id < uart_defs::uarts.size()
                ? uart_defs::uarts[id].get()
                : new uart_defs::uart_async_t();
    uart_defs::debug_uart.init(0x3F8, 4, 115200,
                               data_bits, parity_type, stop_bits);
    return &uart_defs::debug_uart;
}

uart_dev_t *uart_dev_t::open(uint16_t port, uint8_t irq,
                             uint32_t baud, uint8_t data_bits,
                             char parity_type, uint8_t stop_bits,
                             bool polled)
{
    uart_dev_t *uart = polled
            ? static_cast<uart_dev_t*>(new uart_defs::uart_poll_t())
            : static_cast<uart_dev_t*>(new uart_defs::uart_async_t());

    uart_defs::uarts.emplace_back(static_cast<uart_defs::uart_t*>(uart));
    uart->init(port, irq, baud, data_bits, parity_type, stop_bits);

    return uart;
}
