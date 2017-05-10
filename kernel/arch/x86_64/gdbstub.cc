#include "gdbstub.h"
#include "string.h"
#include "functional.h"
#include "vector.h"
#include "mm.h"
#include "printk.h"

#include "device/serial-uart.h"

#include "cpu/idt.h"
#include "cpu/thread_impl.h"
#include "thread.h"
#include "cpu/apic.h"
#include "cpu/halt.h"
#include "cpu/interrupts.h"
#include "cpu/atomic.h"
#include "cpu/except.h"
#include "cpu/control_regs.h"

#define DEBUG_GDBSTUB   1
#if DEBUG_GDBSTUB
#define GDBSTUB_TRACE(...) printdbg("gdbstub: " __VA_ARGS__)
#else
#define TRACE_GDBSTUB(...) ((void)0)
#endif

uint8_t constexpr X86_BREAKPOINT_OPCODE = 0xCC;
static size_t constexpr X86_64_CONTEXT_SIZE = 1120;

enum struct gdb_signal_idx_t {
    NONE,       // 0
    SIGHUP,     // 1
    SIGINT,     // 2
    SIGQUIT,    // 3
    SIGILL,     // 4
    SIGTRAP,    // 5
    SIGABRT,    // 6
    SIGEMT,     // 7
    SIGFPE,     // 8
    SIGKILL,    // 9
    SIGBUS,     // 10
    SIGSEGV,    // 11
    SIGSYS,     // 12
    SIGPIPE,    // 13
    SIGALRM,    // 14
    SIGTERM     // 15
};

enum struct gdb_breakpoint_type_t : uint8_t {
    SOFTWARE = 0,
    HARDWARE = 1,
    WRITEWATCH = 2,
    READWATCH = 3,
    ACCESSWATCH = 4
};

// When a break request comes it, we:
//  - update each RUNNING CPUs state to FREEZING
//  - send an NMI to all other CPUs
//  - each CPU executes the NMI handler, which:
//      - saves the CPU context pointer
//      - updates the CPUs state to 'FROZEN'
//      - goes into a hlt loop, until CPU state is set to RESUMING
//  - the GDB stub polls for them all to reach FROZEN state
//
// To step, the stub sets TF and RF in the target's EFLAGS context
// When a step/continue request comes in, we:
//  - update the target CPUs state to RESUMING
//  - send an NMI IPI to the target CPU to wake it from the hlt
//  - the target CPU reenters the NMI handler and sees its state is not
//    FREEZING, so it returns immediately
//  - the hlt loop polls the state and sees that it is not FROZEN, so
//    it stops executing hlt and returns
//  - the CPU restores the (potentially modified) context from the original
//    NMI and continues

enum struct gdb_cpu_state_t {
    RUNNING,
    FREEZING,
    FROZEN,
    RESUMING,
};

struct gdb_cpu_t {
    uint32_t apic_id;
    int cpu_nr;
    isr_context_t *ctx;
    gdb_cpu_state_t volatile state;

    gdb_cpu_t(uint32_t init_apic_id, int nr)
        : apic_id(init_apic_id)
        , cpu_nr(nr)
    {
    }
};

class gdb_cpu_ctrl_t {
public:
    static isr_context_t *context_of(int cpu_nr);

    static bool is_cpu_running(int cpu_nr);
    static int is_cpu_frozen(int cpu_nr);
    static void freeze_all();
    static void continue_frozen(int cpu_nr, bool single_step);

    static void hook_exceptions();
    static void start_stub();

    static int get_gdb_cpu();

    static gdb_signal_idx_t signal_from_intr(int intr);

    template<typename T>
    bool breakpoint_write_target(uintptr_t addr, T value,
                                 T *old_value, uintptr_t page_dir);

    static bool breakpoint_add(gdb_breakpoint_type_t type,
                               uintptr_t addr, uintptr_t page_dir, int kind);
    static bool breakpoint_del(gdb_breakpoint_type_t type,
                               uintptr_t addr, uintptr_t page_dir);
    static void breakpoint_toggle_all(bool activate);

private:
    struct breakpoint_t {
        // Address
        uintptr_t addr;

        // Memory mapping into which to insert the breakpoint
        uintptr_t page_dir;

        // Hardware/software/etc
        gdb_breakpoint_type_t type;

        // The value of the original byte for breakpoints
        uint8_t orig_data;

        // The width of the type for hardware breakpoints
        uint8_t kind;

        // Which hardware breakpoint it is using
        uint8_t hw_bp_reg;

        // True when it is written into the executable
        bool active;

        breakpoint_t(gdb_breakpoint_type_t type_,
                     uintptr_t addr_, uintptr_t page_dir_,
                     uint8_t kind_, uint8_t orig_data_, bool active_)
            : addr(addr_)
            , page_dir(page_dir_)
            , type(type_)
            , orig_data(orig_data_)
            , kind(kind_)
            , hw_bp_reg(0)
            , active(active_)
        {
        }
    };

    using bp_list = vector<breakpoint_t>;

    bp_list::iterator breakpoint_find(bp_list &list, uintptr_t addr, uintptr_t page_dir);
    bp_list &breakpoint_list(gdb_breakpoint_type_t type);

    bool breakpoint_toggle(breakpoint_t& bp, bool activate);
    void breakpoint_toggle_list(bp_list& list, bool activate);

    inline void start();

    void freeze_one(gdb_cpu_t &cpu);

    static int gdb_thread(void*);

    __noreturn
    inline void gdb_thread();

    // Lookup currently executing cpu (cpu_nr == 0)
    // or specified cpu (cpu_nr > 0)
    gdb_cpu_t *cpu_from_nr(int cpu_nr);

    static void wait(gdb_cpu_t const *cpu);

    static isr_context_t *exception_handler(int, isr_context_t *ctx);

    inline isr_context_t *exception_handler(isr_context_t *ctx);

    static gdb_cpu_ctrl_t instance;

    vector<gdb_cpu_t> cpus;
    thread_t stub_tid;
    int gdb_cpu;
    bool volatile stub_running;

    // Breakpoints
    bp_list bp_sw;
    bp_list bp_hw;
};

gdb_cpu_ctrl_t gdb_cpu_ctrl_t::instance;

class gdbstub_t {
public:
    __noreturn
    void run();

    void *operator new(size_t)
    {
        return calloc(1, sizeof(gdbstub_t));
    }

    void operator delete(void *p)
    {
        free(p);
    }

private:
    enum struct rx_state_t {
        IDLE,
        GETLINE,
        GETLINE_ESCAPED,
        CHKSUM1,
        CHKSUM2
    };

    enum struct gdb_signal_t {
        ZERO = 0,
        INT = 2,
        QUIT = 3,
        TRAP = 5,
        ABRT = 6,
        ALRM = 14,
        IO = 23,
        XCPU = 24,
        UNKNOWN = 143
    };

    enum struct run_state_t {
        STOPPED,
        STEPPING,
        RUNNING
    };

    static size_t constexpr MAX_BUFFER_SIZE = 4096;

    using sender_fn_t = function<ssize_t(char const *, size_t)>;

    // 'g' reply packet is a list of 32-bit values, enumeration is array index
    enum struct reg_t {
        // General registers
        RAX,
        RBX,
        RCX,
        RDX,
        RSI,
        RDI,
        RBP,
        RSP,
        R8,
        R9,
        R10,
        R11,
        R12,
        R13,
        R14,
        R15,
        // Instruction pointer and flags
        RIP,
        EFLAGS,
        // Segment registers
        CS,
        SS,
        DS,
        ES,
        FS,
        GS,
        // 10 bytes per 80-bit FPU register (64 bit mantissa, 16 bit exponent)
        ST0,
        ST1,
        ST2,
        ST3,
        ST4,
        ST5,
        ST6,
        ST7,
        // FPU status/control registers
        FCTRL,
        FSTAT,
        FTAG,
        FISEG,
        FIOFF,
        FOSEG,
        FOOFF,
        FOP,
        // SSE registers
        XMM0,
        XMM1,
        XMM2,
        XMM3,
        XMM4,
        XMM5,
        XMM6,
        XMM7,
        XMM8,
        XMM9,
        XMM10,
        XMM11,
        XMM12,
        XMM13,
        XMM14,
        XMM15,
        MXCSR
    };

    struct reg_info_t {
        // Register name
        char const *name;

        // Size in bytes, translated to size in hex digits
        uint8_t size;

        // Offset in hex digits
        uint16_t offset;
    };

    static reg_info_t regs[];

    // MUST use lowercase hex to avoid ambiuity with Enn error reply
    static char constexpr hexlookup[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    void init_reg_offsets();

    template<typename T>
    static size_t to_hex_bytes(char *buf, T value)
    {
        size_t constexpr n = sizeof(value) << 1;
        size_t i;
        for (i = 0; i < n; i += 2) {
            buf[i+0] = hexlookup[(value >> (1*4)) & 0xF];
            buf[i+1] = hexlookup[(value >> (0*4)) & 0xF];
            value >>= sizeof(T) > 1 ? 8 : 0;
        }
        return i;
    }

    template<typename T>
    static T from_hex(char const **input)
    {
        using U = typename safe_underlying_type<T>::type;
        U value;
        for (value = 0; **input; ++*input) {
            int digit = from_hex(**input);
            if (digit < 0)
                break;
            value <<= 4;
            value |= digit;
        }
        return T(value);
    }

    static int from_hex(char ch)
    {
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        else if (ch >= 'A' && ch <= 'F')
            return ch + 10 - 'A';
        else if (ch >= 'a' && ch <= 'f')
            return ch + 10 - 'a';

        return -1;
    }

    template<typename T>
    static void from_hex(T *out, char const *&input,
                         size_t advance = sizeof(T));

    template<typename T>
    static void from_hex_bytes(T *out, char const *&input,
                               size_t advance = sizeof(T));

    void data_received(char const *data, size_t size);
    bool match(const char *&input, char const *pattern,
               char const *separators, char *sep = nullptr);
    bool parse_memop(uintptr_t& addr, size_t& size, const char *input);


    uint8_t calc_checksum(char const *data, size_t size);

    rx_state_t handle_packet();
    rx_state_t handle_memop_read(const char *input);
    rx_state_t handle_memop_write(const char *&input);

    rx_state_t replyf_hex(char const *format, ...);
    rx_state_t replyf(char const *format, ...);
    rx_state_t reply(char const *data);
    rx_state_t reply_hex(char const *format, size_t size);
    rx_state_t reply(char const *data, size_t size);
    static size_t get_context(char *reply, isr_context_t const *ctx);
    static size_t set_context(isr_context_t const *ctx, char const *data);

    uart_dev_t *port;

    size_t tx_index;
    size_t cmd_index;
    size_t reply_index;

    rx_state_t cmd_state;
    uint8_t cmd_sum;
    uint8_t cmd_checksum;

    // Current CPU for step/continue operations
    uint8_t c_cpu;

    // Current CPU for other operations
    uint8_t g_cpu;

    // Current CPU for q{f|s}ThreadInfo
    uint8_t q_cpu;

    run_state_t run_state;
    int step_cpu_nr;

    // Escaped input/output
    char rx_buf[MAX_BUFFER_SIZE];
    char tx_buf[MAX_BUFFER_SIZE];

    // Unescaped input/output
    char cmd_buf[MAX_BUFFER_SIZE];
    char reply_buf[MAX_BUFFER_SIZE];
};

char constexpr gdbstub_t::hexlookup[];

gdbstub_t::reg_info_t gdbstub_t::regs[] = {
    // General registers
    { "rax",      8, 0 },
    { "rbx",      8, 0 },
    { "rcx",      8, 0 },
    { "rdx",      8, 0 },
    { "rsi",      8, 0 },
    { "rdi",      8, 0 },
    { "rbp",      8, 0 },
    { "rsp",      8, 0 },
    { "r8",       8, 0 },
    { "r9",       8, 0 },
    { "r10",      8, 0 },
    { "r11",      8, 0 },
    { "r12",      8, 0 },
    { "r13",      8, 0 },
    { "r14",      8, 0 },
    { "r15",      8, 0 },
    // Instruction pointer, code segment, stack segment
    { "rip",      8, 0 },
    { "eflags",   4, 0 },
    { "cs",       4, 0 },
    { "ss",       4, 0 },
    // Data segments
    { "ds",       4, 0 },
    { "es",       4, 0 },
    { "fs",       4, 0 },
    { "gs",       4, 0 },
    // FPU/MMX registers
    { "st0",     10, 0 },
    { "st1",     10, 0 },
    { "st2",     10, 0 },
    { "st3",     10, 0 },
    { "st4",     10, 0 },
    { "st5",     10, 0 },
    { "st6",     10, 0 },
    { "st7",     10, 0 },
    // FPU control registers
    { "fcw",      4, 0 },
    { "fstat",    4, 0 },
    { "ftag",     4, 0 },
    { "fiseg",    4, 0 },
    { "fiofs",    4, 0 },
    { "foseg",    4, 0 },
    { "foofs",    4, 0 },
    { "fop",      4, 0 },
    // SSE registers
    { "xmm0",    16, 0 },
    { "xmm1",    16, 0 },
    { "xmm2",    16, 0 },
    { "xmm3",    16, 0 },
    { "xmm4",    16, 0 },
    { "xmm5",    16, 0 },
    { "xmm6",    16, 0 },
    { "xmm7",    16, 0 },
    { "xmm8",    16, 0 },
    { "xmm9",    16, 0 },
    { "xmm10",   16, 0 },
    { "xmm11",   16, 0 },
    { "xmm12",   16, 0 },
    { "xmm13",   16, 0 },
    { "xmm14",   16, 0 },
    { "xmm15",   16, 0 },
    { "mxcsr",    4, 0 },
    { "orig_rax", 8, 0 },
    { "fs_base",  8, 0 },
    { "gs_base",  8, 0 }
};

extern "C" void gdb_init()
{
    gdb_cpu_ctrl_t::start_stub();
}

gdbstub_t::rx_state_t gdbstub_t::replyf_hex(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    tx_index = vsnprintf(tx_buf, MAX_BUFFER_SIZE, format, ap);
    va_end(ap);
    return reply_hex(tx_buf, tx_index);
}

gdbstub_t::rx_state_t gdbstub_t::reply_hex(char const *data, size_t size)
{
    if (data == tx_buf) {
        // In-place expand to hex
        for (size_t i = size; i > 0; --i)
            to_hex_bytes(tx_buf + i * 2 - 2, tx_buf[i-1]);
    } else {
        tx_index = 0;
        for (size_t i = 0; i < size; ++i)
            to_hex_bytes(tx_buf + i * 2, tx_buf[i]);
    }

    return reply(tx_buf, size * 2);
}

gdbstub_t::rx_state_t gdbstub_t::replyf(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    tx_index = vsnprintf(tx_buf, MAX_BUFFER_SIZE, format, ap);
    va_end(ap);
    return reply(tx_buf, tx_index);
}

gdbstub_t::rx_state_t gdbstub_t::reply(const char *data)
{
    return reply(data, strlen(data));
}

gdbstub_t::rx_state_t gdbstub_t::reply(const char *data, size_t size)
{
    //GDBSTUB_TRACE("Unencoded reply: \"%*.*s\"\n", int(size), int(size), data);

    reply_index = 0;
    reply_buf[reply_index++] = '$';

    for (size_t i = 0; i < size; ++i) {
        char c = data[i];

        if (c != '$' && c != '#' && c != '}' && c != '{' && c != '*') {
            reply_buf[reply_index++] = c;

            // Look for repeat counts >= 4 for RLE compression
            int count = 1;
            while (reply_index + count < size &&
                   data[i+count] == c &&
                   count < 126-32)
                ++count;

            if (count < 4)
                continue;

            char encoded_count = count - 4 + ' ';

            // Avoid repeat counts which use protocol character encodings
            while (encoded_count == '#' || encoded_count == '$') {
                --count;
                --encoded_count;
            }

            reply_buf[reply_index++] = '*';
            reply_buf[reply_index++] = encoded_count;
            i += count - 1;
        } else {
            reply_buf[reply_index++] = '}';
            reply_buf[reply_index++] = c ^ 0x20;
        }
    }

    reply_buf[reply_index++] = '#';
    uint8_t checksum = calc_checksum(reply_buf + 1, reply_index - 2);
    reply_index += to_hex_bytes(reply_buf + reply_index, checksum);
    reply_buf[reply_index] = 0;

    GDBSTUB_TRACE("Encoded reply: \"%*.*s\"\n",
                  int(reply_index), int(reply_index), reply_buf);

    char ack = 0;
    do {
        assert(ack == 0 || ack == '-');

        port->write(reply_buf, reply_index, reply_index);

        port->read(&ack, 1, 1);

        GDBSTUB_TRACE("Got reply response: %c\n", ack);
    } while (ack == '-');

    return rx_state_t::IDLE;
}

void gdbstub_t::data_received(char const *data, size_t size)
{
    bool nonsense = false;

    for (size_t i = 0; i < size; ++i) {
        char ch = data[i];

        switch (cmd_state) {
        case rx_state_t::IDLE:
            if (ch == '$') {
                cmd_index = 0;
                cmd_sum = 0;
                cmd_state = rx_state_t::GETLINE;
                nonsense = false;
            } else if (ch == 3) {
                cmd_buf[0] = 3;
                cmd_index = 1;
                cmd_state = handle_packet();
                nonsense = false;
            } else {
                GDBSTUB_TRACE("Dropped nonsense character"
                              " in IDLE state: 0x%x '%c'\n", uint8_t(ch),
                              ch >= 32 && ch < 127 ? ch : '.');
                nonsense = true;
            }
            break;

        case rx_state_t::GETLINE:
            if (unlikely(cmd_index >= MAX_BUFFER_SIZE - 1)) {
                GDBSTUB_TRACE("Command buffer overrun, dropping command\n");
                cmd_state = rx_state_t::IDLE;
            } else if (unlikely(ch == '}')) {
                cmd_sum += ch;
                cmd_state = rx_state_t::GETLINE_ESCAPED;
            } else if (likely(ch != '#')) {
                cmd_sum += ch;
                cmd_buf[cmd_index++] = ch;
            } else {
                cmd_buf[cmd_index] = 0;
                cmd_state = rx_state_t::CHKSUM1;
            }
            break;

        case rx_state_t::GETLINE_ESCAPED:
            cmd_sum += ch;
            cmd_buf[cmd_index++] = ch ^ 0x20;
            cmd_state = rx_state_t::GETLINE;
            break;

        case rx_state_t::CHKSUM1:
            cmd_checksum = from_hex(ch) << 4;
            cmd_state = rx_state_t::CHKSUM2;
            break;

        case rx_state_t::CHKSUM2:
            cmd_checksum |= from_hex(ch);

            if (cmd_sum == cmd_checksum) {
                // Send ACK
                GDBSTUB_TRACE("command: %s\n", cmd_buf);
                port->write("+", 1, 1);
                cmd_state = handle_packet();
                nonsense = false;
            } else {
                // Send NACK
                GDBSTUB_TRACE("Sending NAK, wrong checksum!\n");
                port->write("-", 1, 1);
                cmd_state = rx_state_t::IDLE;
            }

            break;

        }
    }

    if (nonsense) {
        port->write("-", 1, 1);
        cmd_state = rx_state_t::IDLE;
    }
}

void gdbstub_t::run()
{
    // Fork off a private copy of the code so inserted breakpoints
    // will not interfere with the stub
    mm_fork_kernel_text();

    port = uart_dev_t::open(0);

    port->route_irq(gdb_cpu_ctrl_t::get_gdb_cpu());

    c_cpu = 1;
    g_cpu = 1;
    q_cpu = 1;

    init_reg_offsets();

    isr_context_t *ctx;
    gdb_signal_idx_t sig;

    for (;;) {
        ssize_t rcvd = port->read(rx_buf, MAX_BUFFER_SIZE, 0);

        if (rcvd != 0)
            data_received(rx_buf, rcvd);
        else {
            // Poll for a frozen CPU
            int cpu_nr;

            if (run_state == run_state_t::STEPPING)
                cpu_nr = step_cpu_nr;
            else
                cpu_nr = gdb_cpu_ctrl_t::is_cpu_frozen(-1);

            if (run_state == run_state_t::STOPPED || cpu_nr == 0 ||
                    !gdb_cpu_ctrl_t::is_cpu_frozen(cpu_nr)) {
                thread_sleep_for(100);
                continue;
            }

            if (run_state == run_state_t::RUNNING)
                gdb_cpu_ctrl_t::freeze_all();

            run_state = run_state_t::STOPPED;
            step_cpu_nr = 0;

            ctx = gdb_cpu_ctrl_t::context_of(cpu_nr);
            sig = gdb_cpu_ctrl_t::signal_from_intr(ctx->gpr->info.interrupt);

            replyf("T%02xthread:%x;", sig, cpu_nr);
        }
    }
}

void gdbstub_t::init_reg_offsets()
{
    size_t ofs = 0;
    for (size_t i = 0; i < countof(regs); ++i) {
        regs[i].offset = ofs;
        regs[i].size *= 2;
        ofs += regs[i].size;
    }
    assert(ofs == X86_64_CONTEXT_SIZE);
}

bool gdbstub_t::match(char const *&input, char const *pattern,
                      char const *separators, char *sep)
{
    size_t len = strlen(pattern);

    if (!strncmp(input, pattern, len)) {
        if (input[len] == 0 || strchr(separators, input[len])) {
            if (sep)
                *sep = input[len];
            input += len + (input[len] != 0);
            return true;
        }
    }

    return false;
}

bool gdbstub_t::parse_memop(uintptr_t& addr, size_t& size, char const *input)
{
    from_hex<uintptr_t>(&addr, input);
    if (*input++ != ',')
        return false;

    from_hex<size_t>(&size, input);

    if (size + 4 > MAX_BUFFER_SIZE / 2)
        return false;

    return true;
}

gdbstub_t::rx_state_t gdbstub_t::handle_memop_read(char const *input)
{
    uintptr_t memop_addr;
    uint64_t saved_cr3;
    size_t memop_size;
    size_t memop_index;
    uint8_t const *memop_ptr;
    isr_context_t const *ctx;

    if (!parse_memop(memop_addr, memop_size, input))
        return reply("E02");

    memop_ptr = (uint8_t *)memop_addr;

    ctx = gdb_cpu_ctrl_t::context_of(g_cpu);
    saved_cr3 = cpu_get_page_directory();

    __try {
        cpu_set_page_directory(ctx->gpr->cr3);
        cpu_flush_tlb();
        __try {
            for (memop_index = 0; memop_index < memop_size; ++memop_index)
                to_hex_bytes(tx_buf + memop_index*2, memop_ptr[memop_index]);
        }
        __catch {
        }
    }
    __catch {
    }

    cpu_set_page_directory(saved_cr3);
    cpu_flush_tlb();

    if (memop_index)
        return reply(tx_buf, memop_index * 2);

    return reply("E14");
}

gdbstub_t::rx_state_t gdbstub_t::handle_memop_write(char const *&input)
{
    uintptr_t memop_addr;
    uint64_t saved_cr3;
    size_t memop_size;
    size_t memop_index;
    uint8_t *memop_ptr;
    isr_context_t const *ctx;
    bool ok;

    if (!parse_memop(memop_addr, memop_size, input) || *input++ != ':')
        return reply("E02");

    memop_ptr = (uint8_t *)memop_addr;

    ctx = gdb_cpu_ctrl_t::context_of(g_cpu);
    saved_cr3 = cpu_get_page_directory();

    ok = false;

    __try {
        cpu_set_page_directory(ctx->gpr->cr3);
        cpu_flush_tlb();
        __try {
            for (memop_index = 0; memop_index < memop_size; ++memop_index)
                from_hex<uint8_t>(&memop_ptr[memop_index], input);
            ok = true;
        }
        __catch {
        }
    }
    __catch {
    }

    cpu_set_page_directory(saved_cr3);
    cpu_flush_tlb();

    return reply(ok ? "OK" : "E14");
}

gdbstub_t::rx_state_t gdbstub_t::handle_packet()
{
    char const *input = cmd_buf;

    char ch = *input++;

    // The following commands are required for GDB to work:
    //  g/G to get/set register values
    //  m/M to get/set memory values
    //  s/c to step and continue
    //  vCont to step and continue with multiple threads

    isr_context_t *ctx;
    uintptr_t addr;
    int thread;
    int cpu;
    gdb_breakpoint_type_t type;
    int kind;
    bool add;
    size_t reg;

    vector<char> step_actions;

    switch (ch) {
    case 3:
        // Ctrl-c pressed on client
        step_cpu_nr = c_cpu;
        run_state = run_state_t::STEPPING;
        gdb_cpu_ctrl_t::freeze_all();
        return rx_state_t::IDLE;

    case '?':
        return replyf("T%02xthread:%02x;", gdb_signal_t::TRAP, c_cpu);

    case 'g':
        // Get all CPU registers
        ctx = gdb_cpu_ctrl_t::context_of(g_cpu);
        return reply(tx_buf, get_context(tx_buf, ctx));

    case 'G':
        // Set all CPU registers
        ctx = gdb_cpu_ctrl_t::context_of(g_cpu);
        if (ctx) {
            set_context(ctx, input);
            return reply("OK");
        }

        return reply("E03");

    case 'p':
        // Get single CPU register
        ctx = gdb_cpu_ctrl_t::context_of(g_cpu);
        get_context(tx_buf, ctx);
        reg = from_hex<size_t>(&input);
        assert(reg < countof(regs));
        if (reg >= countof(regs))
            return reply("E03");

        return reply(tx_buf + regs[reg].offset, regs[reg].size);

    case 'P':
        // Set single CPU register
        ctx = gdb_cpu_ctrl_t::context_of(g_cpu);
        reg = from_hex<size_t>(&input);

        if (reg >= countof(regs))
            return reply("E03");

        if (*input++ != '=')
            return reply("E03");

        get_context(tx_buf, ctx);
        memset(tx_buf + regs[reg].offset, '0', regs[reg].size);
        assert(strlen(input) >= regs[reg].size);
        memcpy(tx_buf + regs[reg].offset, input, regs[reg].size);
        set_context(ctx, tx_buf);
        return reply("OK");

    case 'm':
        // Read memory
        return handle_memop_read(input);

    case 'M':
        // Set memory
        return handle_memop_write(input);

    case 'H':
        thread = 0;

        cpu = *input++;
        if (cpu != 'c' && cpu != 'g') {
            reply("E03");
            return rx_state_t::IDLE;
        }

        thread = from_hex<int>(&input);

        if (thread > 0) {
            if (cpu == 'c')
                c_cpu = thread;
            else if (cpu == 'g')
                g_cpu = thread;

            GDBSTUB_TRACE("Changed %c CPU to %d\n", cpu, thread);
        }
        return reply("OK");

    case 'q':
        if (!strcmp(input, "C")) {
            return reply("QC1");
        } else if (match(input, "Supported", ":")) {
            return replyf("PacketSize=%x", MAX_BUFFER_SIZE);
        } else if (!strcmp(input, "fThreadInfo") ||
                !strcmp(input, "sThreadInfo")) {
            if (input[0] == 's')
                return reply("l");

            tx_index = 0;
            tx_buf[tx_index++] = 'm';

            for (int i = 0, e = gdb_cpu_ctrl_t::get_gdb_cpu(); i < e; ++i) {
                if (i != 0)
                    tx_buf[tx_index++] = ',';
                tx_index += snprintf(tx_buf + tx_index,
                                     MAX_BUFFER_SIZE - tx_index,
                                     "%x", i + 1);
            }

            return reply(tx_buf, tx_index);
        } else if (!strncmp(input, "ThreadExtraInfo,", 16)) {
            input += 16;
            thread = from_hex<int>(&input);
            bool cpu_running = gdb_cpu_ctrl_t::is_cpu_running(thread);
            return replyf_hex("CPU#%d [%s]", thread,
                              cpu_running ? "running" : "halted");
        }
        break;

    case 'T':
        thread = from_hex<int>(&input);

        if (thread > 0 && thread < gdb_cpu_ctrl_t::get_gdb_cpu())
            return reply("OK");

        return reply("E01");

    case 'v':
        if (!strcmp(input, "Cont?")) {
            // "vCont?" command
            return reply("vCont;s;c;S;C");
        } else if (match(input, "Cont", ":;")) {
            // Initialize an array with one entry per CPU
            // Values are set once, only entries with 0 value are modified
            step_actions.resize(gdb_cpu_ctrl_t::get_gdb_cpu(), 0);

            step_cpu_nr = 0;

            while (*input) {
                char action = 0;
                char sep = 0;

                if (match(input, "c", ":", &sep))
                    action = 'c';
                else if (match(input, "s", ":", &sep))
                    action = 's';
                else
                    break;

                thread = 0;
                if (*input)
                    thread = from_hex<int>(&input);

                if (thread > 0) {
                    // Step a specific CPU
                    if (step_actions.at(thread - 1) == 0) {
                        step_actions[thread - 1] = action;

                        if (action == 's' && step_cpu_nr == 0)
                            step_cpu_nr = thread;
                    }
                } else {
                    // Step all CPUs that don't have an action assigned
                    for (char& cpu_action : step_actions) {
                        if (cpu_action == 0)
                            cpu_action = action;
                    }
                }

                run_state = step_cpu_nr
                        ? run_state_t::STEPPING
                        : run_state_t::RUNNING;

                for (char& cpu_action : step_actions) {
                    if (cpu_action != 0)
                        gdb_cpu_ctrl_t::continue_frozen(
                                    thread, cpu_action == 's');
                }

                return rx_state_t::IDLE;
            }
        }
        break;

    case 'z':
    case 'Z':
        add = (ch == 'Z');

        type = from_hex<gdb_breakpoint_type_t>(&input);
        input += (*input == ',');

        addr = from_hex<uintptr_t>(&input);
        input += (*input == ',');

        kind = from_hex<int>(&input);

        GDBSTUB_TRACE("%s breakpoint at %zx with size %d\n",
                      add ? "add" : "remove", addr, kind);

        ctx = gdb_cpu_ctrl_t::context_of(g_cpu);

        if (add)
            gdb_cpu_ctrl_t::breakpoint_add(type, addr, ctx->gpr->cr3, kind);
        else
            gdb_cpu_ctrl_t::breakpoint_del(type, addr, ctx->gpr->cr3);

        return reply("OK");
    }

    return reply("");
}

uint8_t gdbstub_t::calc_checksum(const char *data, size_t size)
{
    uint8_t checksum = 0;
    for (size_t chk = 0; chk < size; ++chk)
        checksum += data[chk];
    return checksum;
}

size_t gdbstub_t::get_context(char *reply, const isr_context_t *ctx)
{
    size_t ofs = 0;

    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_RAX(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_RBX(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_RCX(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_RDX(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_RSI(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_RDI(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_RBP(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_RSP(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_R8(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_R9(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_R10(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_R11(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_R12(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_R13(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_R14(ctx));
    ofs += to_hex_bytes(reply + ofs, ISR_CTX_REG_R15(ctx));

    ofs += to_hex_bytes(reply + ofs, uintptr_t(ISR_CTX_REG_RIP(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_REG_RFLAGS(ctx)));

    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_REG_CS(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_REG_SS(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_REG_DS(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_REG_ES(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_REG_FS(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_REG_GS(ctx)));

    for (size_t st = 0; st < 8; ++st) {
        ofs += to_hex_bytes(reply + ofs, ISR_CTX_FPU_STn_31_0(ctx, st));
        ofs += to_hex_bytes(reply + ofs, ISR_CTX_FPU_STn_63_32(ctx, st));
        ofs += to_hex_bytes(reply + ofs, ISR_CTX_FPU_STn_79_64(ctx, st));
    }

    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_FPU_FCW(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_FPU_FSW(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_FPU_FTW(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_FPU_FIS(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_FPU_FIP(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_FPU_FDS(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_FPU_FDP(ctx)));
    ofs += to_hex_bytes(reply + ofs, uint32_t(ISR_CTX_FPU_FOP(ctx)));

    for (size_t xmm = 0; xmm < 16; ++xmm) {
        for (size_t i = 0; i < 2; ++i) {
            ofs += to_hex_bytes(reply + ofs, ISR_CTX_SSE_XMMn_q(ctx, xmm, i));
        }
    }

    ofs += to_hex_bytes(reply + ofs, ISR_CTX_SSE_MXCSR(ctx));

    // orig_rax
    memset(reply + ofs, 'x', 16);
    ofs += 16;

    ofs += to_hex_bytes(reply + ofs, uintptr_t(ISR_CTX_FSBASE(ctx)));

    memset(reply + ofs, 'x', 16);
    ofs += 16;

    assert(ofs == X86_64_CONTEXT_SIZE);

    return ofs;
}

template<typename T>
void gdbstub_t::from_hex(T *out, char const *&input, size_t advance)
{
    assert(advance >= sizeof(T));

    *out = 0;

    for (size_t i = 0; i < sizeof(T)*2; ++i) {
        int digit = from_hex(*input);
        if (digit < 0)
            break;
        ++input;
        *out <<= 4;
        *out |= uint8_t(digit);
    }

    if (advance > sizeof(T))
        input += (advance - sizeof(T)) * 2;
}

template<typename T>
void gdbstub_t::from_hex_bytes(T *out, char const *&input, size_t advance)
{
    assert(advance >= sizeof(T));

    *out = 0;

    for (size_t i = 0; i < sizeof(T)*2; i += 2) {
        int hi_digit = from_hex(input[0]);
        if (hi_digit < 0)
            break;

        int lo_digit = from_hex(input[1]);
        if (lo_digit < 0)
            break;

        T value = (hi_digit << 4) | lo_digit;
        *out |= value << (i * 4);

        input += 2;
    }

    if (advance > sizeof(T))
        input += (advance - sizeof(T)) * 2;
}

size_t gdbstub_t::set_context(isr_context_t const *ctx, char const *data)
{
    char const *input = data;

    from_hex_bytes(&ISR_CTX_REG_RAX(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_RBX(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_RCX(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_RDX(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_RSI(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_RDI(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_RBP(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_RSP(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_R8(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_R9(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_R10(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_R11(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_R12(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_R13(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_R14(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_R15(ctx), input);

    from_hex_bytes((uintptr_t*)&ISR_CTX_REG_RIP(ctx), input);
    from_hex_bytes((uint32_t*)&ISR_CTX_REG_RFLAGS(ctx), input);

    from_hex_bytes((uint32_t*)&ISR_CTX_REG_CS(ctx), input);
    from_hex_bytes((uint32_t*)&ISR_CTX_REG_SS(ctx), input);
    from_hex_bytes(&ISR_CTX_REG_DS(ctx), input, sizeof(uint32_t));
    from_hex_bytes(&ISR_CTX_REG_ES(ctx), input, sizeof(uint32_t));
    from_hex_bytes(&ISR_CTX_REG_FS(ctx), input, sizeof(uint32_t));
    from_hex_bytes(&ISR_CTX_REG_GS(ctx), input, sizeof(uint32_t));

    for (size_t st = 0; st < 8; ++st) {
        from_hex_bytes(&ISR_CTX_FPU_STn_31_0(ctx, st), input);
        from_hex_bytes(&ISR_CTX_FPU_STn_63_32(ctx, st), input);
        from_hex_bytes(&ISR_CTX_FPU_STn_79_64(ctx, st), input);
    }

    from_hex_bytes(&ISR_CTX_FPU_FCW(ctx), input, sizeof(uint32_t));
    from_hex_bytes(&ISR_CTX_FPU_FSW(ctx), input, sizeof(uint32_t));
    from_hex_bytes(&ISR_CTX_FPU_FTW(ctx), input, sizeof(uint32_t));
    from_hex_bytes(&ISR_CTX_FPU_FIS(ctx), input, sizeof(uint32_t));
    from_hex_bytes(&ISR_CTX_FPU_FIP(ctx), input, sizeof(uint32_t));
    from_hex_bytes(&ISR_CTX_FPU_FDS(ctx), input, sizeof(uint32_t));
    from_hex_bytes(&ISR_CTX_FPU_FDP(ctx), input, sizeof(uint32_t));
    from_hex_bytes(&ISR_CTX_FPU_FOP(ctx), input, sizeof(uint32_t));

    for (size_t xmm = 0; xmm < 16; ++xmm) {
        for (size_t i = 0; i < 2; ++i) {
            from_hex_bytes(&ISR_CTX_SSE_XMMn_q(ctx, xmm, i), input);
        }
    }

    from_hex_bytes(&ISR_CTX_SSE_MXCSR(ctx), input);

    input += 16;

    from_hex_bytes((uintptr_t*)&ISR_CTX_FSBASE(ctx), input);

    input += 16;

    assert(input - data == X86_64_CONTEXT_SIZE);

    return input - data;
}

gdb_cpu_ctrl_t::bp_list::iterator gdb_cpu_ctrl_t::breakpoint_find(
        bp_list &list, uintptr_t addr, uintptr_t page_dir)
{
    return find_if(list.begin(), list.end(), [&](breakpoint_t const& item) {
        return item.addr == addr && (!page_dir || item.page_dir == page_dir);
    });
}

gdb_cpu_ctrl_t::bp_list &gdb_cpu_ctrl_t::breakpoint_list(
        gdb_breakpoint_type_t type)
{
    switch (type) {
    case gdb_breakpoint_type_t::SOFTWARE: return bp_sw;
    case gdb_breakpoint_type_t::HARDWARE: return bp_hw;
    case gdb_breakpoint_type_t::READWATCH: return bp_hw;
    case gdb_breakpoint_type_t::WRITEWATCH: return bp_hw;
    case gdb_breakpoint_type_t::ACCESSWATCH: return bp_hw;
    default: panic("Invalid breakpoint type");
    }
}

template<typename T>
bool gdb_cpu_ctrl_t::breakpoint_write_target(
        uintptr_t addr, T value, T* old_value, uintptr_t page_dir)
{
    if (!mpresent(addr))
        return false;

    uintptr_t orig_pagedir = cpu_get_page_directory();
    GDBSTUB_TRACE("Switching from stub pagedir (%zx)"
                  " to target pagedir (%zx)\n", orig_pagedir, page_dir);

    cpu_set_page_directory(page_dir);
    cpu_flush_tlb();
    cpu_cr0_change_bits(CR0_WP, 0);
    *old_value = atomic_xchg((uint8_t*)addr, value);
    cpu_cr0_change_bits(0, CR0_WP);
    cpu_set_page_directory(orig_pagedir);
    cpu_flush_tlb();
    GDBSTUB_TRACE("Switched back to stub pagedir (%zx)\n", orig_pagedir);

    return true;
}

bool gdb_cpu_ctrl_t::breakpoint_add(
        gdb_breakpoint_type_t type, uintptr_t addr,
        uintptr_t page_dir, int kind)
{
    auto& list = instance.breakpoint_list(type);
    auto it = instance.breakpoint_find(list, addr, page_dir);

    // Adding a duplicate breakpoint should be idempotent
    if (it != list.end())
        return true;

    list.emplace_back(type, addr, page_dir, kind, 0, false);

    if (!instance.breakpoint_toggle(list.back(), true)) {
        list.pop_back();
        return false;
    }

    return true;
}

bool gdb_cpu_ctrl_t::breakpoint_del(gdb_breakpoint_type_t type,
                                    uintptr_t addr, uintptr_t page_dir)
{
    auto& list = instance.breakpoint_list(type);
    auto it = instance.breakpoint_find(list, addr, page_dir);
    if (it != list.end()) {
        if (!instance.breakpoint_toggle(*it, false))
            return false;

        list.erase(it);
    }
    return true;
}

bool gdb_cpu_ctrl_t::breakpoint_toggle(breakpoint_t &bp, bool activate)
{
    uint8_t old_value;

    if (activate && !bp.active) {
        if (!breakpoint_write_target(bp.addr, X86_BREAKPOINT_OPCODE,
                                     &bp.orig_data, bp.page_dir))
            return false;
    } else if (!activate && bp.active) {
        if (!breakpoint_write_target(bp.addr, bp.orig_data,
                                     &old_value, bp.page_dir))
            return false;
        assert(old_value == X86_BREAKPOINT_OPCODE);
    }
    return true;
}

void gdb_cpu_ctrl_t::breakpoint_toggle_all(bool activate)
{
    instance.breakpoint_toggle_list(instance.bp_sw, activate);
    instance.breakpoint_toggle_list(instance.bp_hw, activate);
}

void gdb_cpu_ctrl_t::breakpoint_toggle_list(bp_list &list, bool activate)
{
    for (breakpoint_t& bp : list)
        breakpoint_toggle(bp, activate);
}


isr_context_t *gdb_cpu_ctrl_t::context_of(int cpu_nr)
{
    gdb_cpu_t const *cpu = instance.cpu_from_nr(cpu_nr);
    return cpu ? cpu->ctx : nullptr;
}

bool gdb_cpu_ctrl_t::is_cpu_running(int cpu_nr)
{
    gdb_cpu_t const *cpu = instance.cpu_from_nr(cpu_nr);
    return cpu && cpu->state == gdb_cpu_state_t::RUNNING;
}

// if cpu_nr < 0, returns frozen CPUs cpu_nr if any CPU is frozen
// if cpu_nr = 0, returns cpu_nr if frozen
// returns 0 if specified cpus are frozen
int gdb_cpu_ctrl_t::is_cpu_frozen(int cpu_nr)
{
    if (cpu_nr >= 0) {
        gdb_cpu_t const *cpu = instance.cpu_from_nr(cpu_nr);
        return (cpu && cpu->state == gdb_cpu_state_t::FROZEN)
                ? cpu->cpu_nr
                : 0;
    }

    for (gdb_cpu_t& cpu : instance.cpus) {
        if (cpu.state == gdb_cpu_state_t::FROZEN)
            return cpu.cpu_nr;
    }

    return 0;
}

void gdb_cpu_ctrl_t::freeze_all()
{
    for (auto& cpu : instance.cpus)
        instance.freeze_one(cpu);
}

void gdb_cpu_ctrl_t::continue_frozen(int cpu_nr, bool single_step)
{
    for (gdb_cpu_t& cpu : instance.cpus) {
        if (cpu_nr > 0 && cpu.cpu_nr != cpu_nr)
            continue;

        if (cpu.state == gdb_cpu_state_t::FROZEN) {
            // Set trap and resume flag if single stepping
            if (single_step)
                cpu.ctx->gpr->iret.rflags |= EFLAGS_TF | EFLAGS_RF;
            else
                cpu.ctx->gpr->iret.rflags &= ~EFLAGS_TF;

            // Change state to break it out of the halt loop
            cpu.state = gdb_cpu_state_t::RESUMING;

            // Send an NMI to the CPU to wake it up from halt
            apic_send_ipi(cpu.apic_id, INTR_EX_NMI);

            // Wait for CPU to pick it up
            while (cpu.state == gdb_cpu_state_t::RESUMING)
                pause();
        }
    }
}

void gdb_cpu_ctrl_t::hook_exceptions()
{
    intr_hook(INTR_EX_DEBUG, &gdb_cpu_ctrl_t::exception_handler);
    intr_hook(INTR_EX_NMI, &gdb_cpu_ctrl_t::exception_handler);
    intr_hook(INTR_EX_BREAKPOINT, &gdb_cpu_ctrl_t::exception_handler);
    idt_set_unhandled_exception_handler(&gdb_cpu_ctrl_t::exception_handler);

    idt_clone_debug_exception_dispatcher();
}

void gdb_cpu_ctrl_t::start_stub()
{
    instance.start();
}

int gdb_cpu_ctrl_t::get_gdb_cpu()
{
    return instance.gdb_cpu;
}

gdb_signal_idx_t gdb_cpu_ctrl_t::signal_from_intr(int intr)
{
    switch (intr) {
    case INTR_EX_DIV        : return gdb_signal_idx_t::SIGFPE;
    case INTR_EX_DEBUG      : return gdb_signal_idx_t::SIGTRAP;
    case INTR_EX_NMI        : return gdb_signal_idx_t::SIGTRAP;
    case INTR_EX_BREAKPOINT : return gdb_signal_idx_t::SIGTRAP;
    case INTR_EX_OVF        : return gdb_signal_idx_t::SIGFPE;
    case INTR_EX_BOUND      : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_OPCODE     : return gdb_signal_idx_t::SIGILL;
    case INTR_EX_DEV_NOT_AV : return gdb_signal_idx_t::SIGEMT;
    case INTR_EX_DBLFAULT   : return gdb_signal_idx_t::SIGTERM;
    case INTR_EX_COPR_SEG   : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_TSS        : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_SEGMENT    : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_STACK      : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_GPF        : return gdb_signal_idx_t::SIGILL;
    case INTR_EX_PAGE       : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_MATH       : return gdb_signal_idx_t::SIGFPE;
    case INTR_EX_ALIGNMENT  : return gdb_signal_idx_t::SIGTRAP;
    case INTR_EX_MACHINE    : return gdb_signal_idx_t::SIGBUS;
    case INTR_EX_SIMD       : return gdb_signal_idx_t::SIGFPE;
    case INTR_EX_VIRTUALIZE : return gdb_signal_idx_t::SIGBUS;
    default                 : return gdb_signal_idx_t::SIGTRAP;
    }
}

void gdb_cpu_ctrl_t::start()
{
    hook_exceptions();

    int cpu_count = thread_cpu_count();

    cpus.reserve(cpu_count - 1);

    // Dedicate the last CPU to qemu stub
    for (int cpu = 0; cpu < cpu_count - 1; ++cpu) {
        uint32_t apic_id = thread_get_cpu_apic_id(cpu);
        cpus.emplace_back(apic_id, cpu + 1);
    }

    stub_tid = thread_create(gdb_thread, 0, 0, 0);

    while (!stub_running)
        pause();
}

void gdb_cpu_ctrl_t::freeze_one(gdb_cpu_t& cpu)
{
    if (cpu.state == gdb_cpu_state_t::RUNNING) {
        cpu.state = gdb_cpu_state_t::FREEZING;

        apic_send_ipi(cpu.apic_id, INTR_EX_NMI);

        while (cpu.state != gdb_cpu_state_t::FROZEN)
            pause();
    }
}

int gdb_cpu_ctrl_t::gdb_thread(void *)
{
    instance.gdb_thread();
}

void gdb_cpu_ctrl_t::gdb_thread()
{
    while (stub_tid == 0)
        thread_sleep_for(10);

    gdb_cpu = cpus.size();

    thread_set_affinity(stub_tid, 1U << (gdb_cpu));

    // Set GDB stub to time critical priority
    thread_set_priority(stub_tid, 32767);

    freeze_all();

    stub_running = true;

    unique_ptr<gdbstub_t> stub(new gdbstub_t);
    stub->run();
}

// Pass 0 for current CPU
gdb_cpu_t *gdb_cpu_ctrl_t::cpu_from_nr(int cpu_nr)
{
    if (cpu_nr <= 0)
        cpu_nr = thread_cpu_number() + 1;

    auto it = find_if(cpus.begin(), cpus.end(), [&](gdb_cpu_t const& cpu) {
            return cpu.cpu_nr == cpu_nr;
});

    return likely(it != cpus.end()) ? &*it : nullptr;
}

isr_context_t *gdb_cpu_ctrl_t::exception_handler(int, isr_context_t *ctx)
{
    return instance.exception_handler(ctx);
}

void gdb_cpu_ctrl_t::wait(gdb_cpu_t const *cpu)
{
    while (cpu->state == gdb_cpu_state_t::FROZEN)
        halt();
}

isr_context_t *gdb_cpu_ctrl_t::exception_handler(isr_context_t *ctx)
{
    gdb_cpu_t *cpu = cpu_from_nr(0);

    static uintptr_t bp_workaround_addr;

    if (!cpu) {
        // This is the GDB stub
        if (ctx->gpr->iret.rflags & EFLAGS_TF) {
            // We are in a single step breakpoint workaround
            assert(bp_workaround_addr != 0);

            // Find the breakpoint we stepped over
            bp_list::iterator it = breakpoint_find(bp_sw,
                                                   bp_workaround_addr, 0);
            if (it == bp_sw.end())
                return 0;

            bp_workaround_addr = 0;

            // Disable single-step
            ctx->gpr->iret.rflags &= ~EFLAGS_TF;

            // Reenable it
            breakpoint_toggle(*it, true);
            return ctx;
        }

        if (ctx->gpr->info.interrupt == INTR_EX_BREAKPOINT) {
            // Handle hitting breakpoint in stub by deactivating it,
            // single-stepping stepping, and reenabling it (above)

            // Adjust RIP back to start of instruction
            ctx->gpr->iret.rip = (int(*)(void*))((char*)ctx->gpr->iret.rip - 1);

            bp_workaround_addr = uintptr_t(ctx->gpr->iret.rip);

            // Find the breakpoint
            bp_list::iterator it = breakpoint_find(
                        bp_sw, uintptr_t(ctx->gpr->iret.rip), 0);
            if (it == bp_sw.end())
                return 0;

            // Disable it
            breakpoint_toggle(*it, false);

            // Single step the instruction
            ctx->gpr->iret.rflags |= EFLAGS_TF | EFLAGS_RF;

            return ctx;
        }

        return nullptr;
    }

    // Ignore NMI when not freezing
    if (ctx->gpr->info.interrupt == INTR_EX_NMI &&
            cpu->state != gdb_cpu_state_t::FREEZING)
        return ctx;

    cpu->ctx = ctx;

    // GDB thread waits for the state to transition to FROZEN
    cpu->state = gdb_cpu_state_t::FROZEN;

    // Idle the CPU until NMI wakes it
    wait(cpu);

    // The only way to reach here
    assert(cpu->state == gdb_cpu_state_t::RESUMING);

    cpu->ctx = nullptr;

    // Only running threads can be transitioned back to FREEZING
    cpu->state = gdb_cpu_state_t::RUNNING;

    // Continue execution with potentially modified context
    return ctx;
}
