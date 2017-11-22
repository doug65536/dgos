#include "serial-uart.h"
#include "cpu/ioport.h"
#include "callout.h"
#include "vector.h"
#include "irq.h"
#include "threadsync.h"
#include "cpu/control_regs.h"

#define DEBUG_UART  1
#if DEBUG_UART
#define UART_TRACE(...) printdbg("uart: " __VA_ARGS__)
#else
#define UART_TRACE(...) ((void)0)
#endif

// Null model connections:
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
    ONE = 2,
    ZERO = 3
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
    ~uart_t();

protected:
    void init(ioport_t port, uint8_t port_irq, uint32_t baud);

    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);

    virtual isr_context_t *port_irq_handler(isr_context_t *ctx);

    uint16_t queue_next(uint16_t value) const;

    void rx_enqueue(uint16_t value);

    bool is_rx_full() const;
    bool is_tx_full() const;
    bool is_tx_not_empty() const;
    bool is_rx_not_empty() const;

    uint16_t tx_peek_value() const;
    void tx_take_value();

    void send_some_locked();

    __always_inline void out(port_t ofs, uint8_t value) const;
    __always_inline void outs(port_t ofs, const void *value, size_t len) const;

    template<typename T>
    __always_inline void outp(T const& reg)
    {
        out(reg_t<T>::ofs, reg.value);
    }

    __always_inline uint8_t in(port_t ofs)
    {
        return inb(io_base + ioport_t(ofs));
    }

    template<typename T>
    __always_inline T inp(T& reg)
    {
        reg.value = in(reg_t<T>::ofs);
        return reg;
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

    chiptype_t chip;
    uint8_t fifo_size;
};

static vector<unique_ptr<uart_t>> uarts;

uart_t::uart_t()
    : rx_head(0)
    , rx_tail(0)
    , io_base(0)
    , irq(0)
    , chip(chiptype_t::UNKNOWN)
    , fifo_size(0)
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

void uart_t::init(ioport_t port, uint8_t port_irq, uint32_t baud)
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
    } else if (reg_fcr.bits.rx_trigger == 3 && !reg_fcr.bits.fifo64) {
        // Both rx_trigger bits were writable, it is a good 16550A
        // Good 16550A
        chip = chiptype_t::U16550A;
        fifo_size = 16;
    } else if (reg_fcr.bits.rx_trigger == 1) {
        // Bit 6 was not preserved, is a crap 16550
        // Buggy 16550
        chip = chiptype_t::U16550;
        fifo_size = 1;
    } else {
        // If the scratch register preserves value, it is a 16450
        reg_scr.value = 0x55;
        outp(reg_scr);
        inp(reg_scr);

        if (reg_scr.value == 0x55)
            chip = chiptype_t::U16450;
        else
            chip = chiptype_t::U8250;

        fifo_size = 1;
    }

    // Enable access to baud rate registers
    reg_lcr.bits.baud_latch = 1;

    // Might as well program the rest of LCR here
    reg_lcr.bits.wordlen = uint8_t(lcr_wordlen_t::DATA8);
    reg_lcr.bits.stopbits = uint8_t(lcr_stopbits_t::STOP1);
    reg_lcr.bits.parity = uint8_t(lcr_parity_t::ZERO);
    reg_lcr.bits.parity_en = 0;
    reg_lcr.bits.tx_break = 0;

    outp(reg_lcr);

    // Set baud rate
    uint16_t divisor = 115200 / baud;
    out(port_t::DLL, (divisor >> (0*8)) & 0xFF);
    out(port_t::DLM, (divisor >> (1*8)) & 0xFF);

    // Disable access to baud rate registers
    reg_lcr.bits.baud_latch = 0;
    outp(reg_lcr);

    // Configure and reset FIFO
    reg_fcr.value = 0;
    reg_fcr.bits.fifo_en = 1;
    reg_fcr.bits.fifo_rx_reset = 1;
    reg_fcr.bits.fifo_tx_reset = 1;
    reg_fcr.bits.rx_trigger = uint8_t(fcr_rx_trigger_t::FIFO8);
    outp(reg_fcr);

    // Configure interrupts
    reg_ier.value = 0;
    reg_ier.bits.error = 1;
    reg_ier.bits.status = 1;
    reg_ier.bits.rx_avail = 1;
    reg_ier.bits.tx_empty = 1;
    outp(reg_ier);

    // Configure modem control outputs
    reg_mcr.value = 0;
    reg_mcr.bits.dtr = 1;
    reg_mcr.bits.int_en = 1;
    reg_mcr.bits.rts = 1;
    outp(reg_mcr);

    irq_setmask(irq, true);
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
        if (uart->irq == irq)
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
    outsb(io_base + ioport_t(ofs), value, len);
}

// ===========================================================================
// IRQ driven, asynchronous UART implementation

class uart_async_t : public uart_t {
public:
    uart_async_t();

    ssize_t write(void const *buf, size_t size, size_t min_write);
    ssize_t read(void *buf, size_t size, size_t min_read);

    void route_irq(int cpu);

    ~uart_async_t();

private:
    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    isr_context_t *port_irq_handler(isr_context_t *ctx);

    void send_some_locked();

    uint16_t queue_next(uint16_t value) const;
    void rx_enqueue(uint16_t value);

    bool is_rx_full() const;
    bool is_tx_full() const;
    bool is_tx_not_empty() const;
    bool is_rx_not_empty() const;
    uint16_t tx_peek_value() const;
    void tx_take_value();

    condition_var_t tx_not_full;
    condition_var_t rx_not_empty;

    // Use 16 bit values to allow buffering of error information
    // as values >= 256
    unique_ptr<uint16_t> rx_buffer;
    unique_ptr<uint16_t> tx_buffer;

    spinlock_t lock;

    uint16_t tx_head;
    uint16_t tx_tail;

    uint8_t log2_buffer_size;

    bool sending_break;
    bool sending_data;
};

uart_async_t::uart_async_t()
    : uart_t()
    , lock(0)
    , tx_head(0)
    , tx_tail(0)
    , log2_buffer_size(16)
    , sending_break(false)
    , sending_data(false)
{
    condvar_init(&tx_not_full);
    condvar_init(&rx_not_empty);

    rx_buffer = new uint16_t[1 << log2_buffer_size];
    tx_buffer = new uint16_t[1 << log2_buffer_size];

    irq_hook(irq, &uart_t::irq_handler);
}

uart_async_t::~uart_async_t()
{
    irq_setmask(irq, false);
    irq_unhook(irq, &uart_t::irq_handler);

    condvar_destroy(&tx_not_full);
    condvar_destroy(&rx_not_empty);
}

ssize_t uart_async_t::write(void const *buf, size_t size, size_t min_write)
{
    spinlock_lock_noirq(&lock);

    auto data = (char const *)buf;

    size_t i;
    for (i = 0; i < size; ++i) {
        if (is_tx_full()) {
            if (i >= min_write)
                break;

            do {
                if (!sending_data)
                    send_some_locked();

                condvar_wait_spinlock(&tx_not_full, &lock);
            } while (is_tx_full());
        }

        tx_buffer[tx_head] = *data++;
        tx_head = queue_next(tx_head);
    }

    if (!sending_data)
        send_some_locked();

    spinlock_unlock_noirq(&lock);

    return i;
}

ssize_t uart_async_t::read(void *buf, size_t size, size_t min_read)
{
    cpu_scoped_irq_disable intr_was_enabled;
    spinlock_lock_noirq(&lock);

    auto data = (char *)buf;

    size_t i;
    for (i = 0; i < size; ++i) {
        if (!is_rx_not_empty()) {
            // Reset buffer to maximize locality
            rx_head = 0;
            rx_tail = 0;

            if (i >= min_read)
                break;

            do {
                condvar_wait_spinlock(&rx_not_empty, &lock);
            } while (!is_rx_not_empty());
        }

        *data++ = rx_buffer[rx_tail];
        rx_tail = queue_next(rx_tail);
    }

    spinlock_unlock_noirq(&lock);

    return i;
}

void uart_async_t::route_irq(int cpu)
{
    irq_setcpu(irq, cpu);
}

void uart_async_t::send_some_locked()
{
    sending_data = is_tx_not_empty();

    for (size_t i = 0; i < 16 && is_tx_not_empty(); ++i) {
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

            // Don't put more data into Tx FIFO after a break byte
            i = 16;

            break;
        }
    }

    if (tx_head == tx_tail) {
        // Reset buffer pointers to maximize locality
        tx_head = 0;
        tx_tail = 0;
    }
}

uint16_t uart_async_t::queue_next(uint16_t value) const
{
    return (value + 1) & ~-(1 << log2_buffer_size);
}

void uart_async_t::rx_enqueue(uint16_t value)
{
    rx_buffer[rx_head] = value;
    rx_head = queue_next(rx_head);
}

bool uart_async_t::is_rx_full() const
{
    // Newly received bytes are placed at rx_head
    // Stream reads take bytes from rx_tail
    return queue_next(rx_head) == rx_tail;
}

bool uart_async_t::is_tx_full() const
{
    // Stream writes place bytes at tx_head
    // Transmit takes bytes from tx_tail
    return queue_next(tx_head) == tx_tail;
}

bool uart_async_t::is_tx_not_empty() const
{
    return tx_head != tx_tail;
}

bool uart_async_t::is_rx_not_empty() const
{
    return rx_head != rx_tail;
}

uint16_t uart_async_t::tx_peek_value() const
{
    return tx_buffer[tx_tail];
}

void uart_async_t::tx_take_value()
{
    tx_tail = queue_next(tx_tail);
}

isr_context_t *uart_async_t::port_irq_handler(isr_context_t *ctx)
{
    spinlock_lock_noyield(&lock);

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
            send_some_locked();
            wake_tx = true;

            break;

        case uint8_t(iir_source_t::STATUS_CHG):
            inp(reg_msr);
            break;

        }
    }

    spinlock_unlock_noirq(&lock);

    if (wake_tx)
        condvar_wake_all(&tx_not_full);

    if (wake_rx)
        condvar_wake_all(&rx_not_empty);

    return ctx;
}


// ===========================================================================
// Polling driven synchronous UART

class uart_poll_t : public uart_t {
    void init(ioport_t port, uint8_t port_irq, uint32_t baud);
    virtual ssize_t write(const void *buf, size_t size, size_t min_write);
    virtual ssize_t read(void *buf, size_t size, size_t min_read);
};

void uart_poll_t::init(ioport_t port, uint8_t port_irq, uint32_t baud)
{
    uart_t::init(port, port_irq, baud);
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

        outs(port_t::DAT, buf + i, block_size);
        i += block_size;
    }
}

ssize_t uart_poll_t::read(void *buf, size_t size, size_t min_read)
{

}

static uart_poll_t debug_uart;

}   // namespace


// ===========================================================================
//

uart_dev_t *uart_dev_t::open(size_t id, bool simple)
{
    if (!simple)
        return id < uart_defs::uarts.size()
                ? uart_defs::uarts[id].get()
                : nullptr;
    return &uart_defs::debug_uart;
}
