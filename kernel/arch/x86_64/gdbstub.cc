#include "gdbstub.h"
#include "string.h"
#include "functional.h"
#include "vector.h"
#include "mm.h"
#include "printk.h"
#include "inttypes.h"

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

#define GDBSTUB_FORCE_FULL_CTX 0

#define DEBUG_GDBSTUB   1
#if DEBUG_GDBSTUB
#define GDBSTUB_TRACE(...) printdbg("gdbstub: " __VA_ARGS__)
#else
#define GDBSTUB_TRACE(...) ((void)0)
#endif

static uint8_t constexpr X86_BREAKPOINT_OPCODE = 0xCC;
static uint8_t constexpr X86_MAX_HW_BP = 4;

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
    SYNC_HW_BP
};

struct gdb_cpu_t {
    int cpu_nr;
    uint32_t apic_id;
    isr_context_t *ctx;
    gdb_cpu_state_t volatile state;
    function<void()> sync_bp;

    // Ignore steps within this range and immediately single step again
    uintptr_t range_step_st;
    uintptr_t range_step_en;

    gdb_cpu_t(uint32_t init_apic_id, int nr)
        : cpu_nr(nr)
        , apic_id(init_apic_id)
        , ctx(nullptr)
        , state(gdb_cpu_state_t::RUNNING)
    {
    }
};

class gdb_cpu_ctrl_t {
public:
    static isr_context_t *context_of(int cpu_nr);

    static bool is_cpu_running(int cpu_nr);
    static int is_cpu_frozen(int cpu_nr);
    static void freeze_all(int first = 1);
    static void continue_frozen(int cpu_nr, bool single_step);
    static void set_step_range(int cpu_nr, uintptr_t st, uintptr_t en);
    static void sync_hw_bp();

    static void hook_exceptions();

    static void start_stub();

    static int get_gdb_cpu();

    static gdb_signal_idx_t signal_from_intr(int intr);
    static char const *signal_name(gdb_signal_idx_t sig);

    template<typename T>
    bool breakpoint_write_target(uintptr_t addr, T value,
                                 T *old_value, uintptr_t page_dir);

    static bool breakpoint_add(gdb_breakpoint_type_t type,
                               uintptr_t addr, uintptr_t page_dir,
                               uint8_t size);
    static bool breakpoint_del(gdb_breakpoint_type_t type,
                               uintptr_t addr, uintptr_t page_dir,
                               uint8_t kind);
    static void breakpoint_toggle_all(bool activate);

    static int breakpoint_get_byte(const uint8_t *addr, uintptr_t page_dir);
    static bool breakpoint_set_byte(uint8_t *addr, uintptr_t page_dir,
                                    uint8_t value);

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

    bp_list::iterator breakpoint_find(bp_list &list, uintptr_t addr,
                                      uintptr_t page_dir, uint8_t kind);
    bp_list &breakpoint_list(gdb_breakpoint_type_t type);

    bool breakpoint_toggle(breakpoint_t& bp, bool activate);
    void breakpoint_toggle_list(bp_list& list, bool activate);

    void start();

    void freeze_one(gdb_cpu_t &cpu);

    _noreturn
    static int gdb_thread(void*);

    _noreturn
    _always_inline void gdb_thread();

    // Lookup currently executing cpu (cpu_nr == 0)
    // or specified cpu (cpu_nr > 0)
    gdb_cpu_t *cpu_from_nr(int cpu_nr);

    static void wait(gdb_cpu_t const *cpu);

    static isr_context_t *exception_handler(int, isr_context_t *ctx);

    _always_inline isr_context_t *exception_handler(isr_context_t *ctx);

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
    _noreturn
    void run();

    void *operator new(size_t) noexcept
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

    struct step_action_t {
        enum type_t {
            NONE,
            CONT,
            STEP,
            RANGE
        };

        type_t type;

        // Range [start,end)
        uintptr_t start;
        uintptr_t end;
    };

    static size_t constexpr MAX_BUFFER_SIZE = 8192 - _MALLOC_OVERHEAD;

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
        MXCSR,
        FSBASE,
        GSBASE,
        // The following exist if AVX is supported
        YMM0H,
        YMM1H,
        YMM2H,
        YMM3H,
        YMM4H,
        YMM5H,
        YMM6H,
        YMM7H,
        YMM8H,
        YMM9H,
        YMM10H,
        YMM11H,
        YMM12H,
        YMM13H,
        YMM14H,
        YMM15H,
        // The following exist if AVX512 is supported and are 256 bits each
        ZMM0H,
        ZMM1H,
        ZMM2H,
        ZMM3H,
        ZMM4H,
        ZMM5H,
        ZMM6H,
        ZMM7H,
        ZMM8H,
        ZMM9H,
        ZMM10H,
        ZMM11H,
        ZMM12H,
        ZMM13H,
        ZMM14H,
        ZMM15H,
        // The following exist if AVX512 is supported and are 512 bits each
        ZMM16,
        ZMM17,
        ZMM18,
        ZMM19,
        ZMM20,
        ZMM21,
        ZMM22,
        ZMM23,
        ZMM24,
        ZMM25,
        ZMM26,
        ZMM27,
        ZMM28,
        ZMM29,
        ZMM30,
        ZMM31
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
    bool parse_memop(uintptr_t& addr, size_t& size, const char *&input);


    uint8_t calc_checksum(char const *data, size_t size);

    rx_state_t handle_packet();
    rx_state_t handle_memop_read(char const *input);
    rx_state_t handle_memop_write(char const *&input);
    bool get_target_desc(unique_ptr<char[]> &result, size_t &result_sz,
                                const char *annex, size_t annex_sz);
    rx_state_t handle_query_features(char const *input);

    rx_state_t replyf_hex(char const *format, ...);
    rx_state_t replyf(char const *format, ...);
    rx_state_t reply(char const *data);
    rx_state_t reply_hex(char const *format, size_t size);
    rx_state_t reply(char const *data, size_t size);

    template<typename T>
    static void append_ctx_reply(char *reply, int& ofs, T *value_ptr);

    size_t get_context(char *reply, isr_context_t const *ctx);
    size_t set_context(isr_context_t *ctx, char const *data, size_t data_len);

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

    int encoded_ctx_sz;

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
    // SSE registers 127:0
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
    // segment bases
    { "fs_base",  8, 0 },
    { "gs_base",  8, 0 },
    // AVX registers 255:128
    { "ymm0h",   16, 0 },
    { "ymm1h",   16, 0 },
    { "ymm2h",   16, 0 },
    { "ymm3h",   16, 0 },
    { "ymm4h",   16, 0 },
    { "ymm5h",   16, 0 },
    { "ymm6h",   16, 0 },
    { "ymm7h",   16, 0 },
    { "ymm8h",   16, 0 },
    { "ymm9h",   16, 0 },
    { "ymm10h",  16, 0 },
    { "ymm11h",  16, 0 },
    { "ymm12h",  16, 0 },
    { "ymm13h",  16, 0 },
    { "ymm14h",  16, 0 },
    { "ymm15h",  16, 0 },
    // AVX-512 extra registers 127:0
    { "xmm16",   16, 0 },
    { "xmm17",   16, 0 },
    { "xmm18",   16, 0 },
    { "xmm19",   16, 0 },
    { "xmm20",   16, 0 },
    { "xmm21",   16, 0 },
    { "xmm22",   16, 0 },
    { "xmm23",   16, 0 },
    { "xmm24",   16, 0 },
    { "xmm25",   16, 0 },
    { "xmm26",   16, 0 },
    { "xmm27",   16, 0 },
    { "xmm28",   16, 0 },
    { "xmm29",   16, 0 },
    { "xmm30",   16, 0 },
    { "xmm31",   16, 0 },
    // AVX-512 extra registers 255:128
    { "ymm16h",  16, 0 },
    { "ymm17h",  16, 0 },
    { "ymm18h",  16, 0 },
    { "ymm19h",  16, 0 },
    { "ymm20h",  16, 0 },
    { "ymm21h",  16, 0 },
    { "ymm22h",  16, 0 },
    { "ymm23h",  16, 0 },
    { "ymm24h",  16, 0 },
    { "ymm25h",  16, 0 },
    { "ymm26h",  16, 0 },
    { "ymm27h",  16, 0 },
    { "ymm28h",  16, 0 },
    { "ymm29h",  16, 0 },
    { "ymm30h",  16, 0 },
    { "ymm31h",  16, 0 },
    // AVX-512 mask registers 63:0
    { "k0",       8, 0 },
    { "k1",       8, 0 },
    { "k2",       8, 0 },
    { "k3",       8, 0 },
    { "k4",       8, 0 },
    { "k5",       8, 0 },
    { "k6",       8, 0 },
    { "k7",       8, 0 },
    // AVX-512 registers 511:256
    { "zmm0h",   32, 0 },
    { "zmm1h",   32, 0 },
    { "zmm2h",   32, 0 },
    { "zmm3h",   32, 0 },
    { "zmm4h",   32, 0 },
    { "zmm5h",   32, 0 },
    { "zmm6h",   32, 0 },
    { "zmm7h",   32, 0 },
    { "zmm8h",   32, 0 },
    { "zmm9h",   32, 0 },
    { "zmm10h",  32, 0 },
    { "zmm11h",  32, 0 },
    { "zmm12h",  32, 0 },
    { "zmm13h",  32, 0 },
    { "zmm14h",  32, 0 },
    { "zmm15h",  32, 0 },
    { "zmm16h",  32, 0 },
    { "zmm17h",  32, 0 },
    { "zmm18h",  32, 0 },
    { "zmm19h",  32, 0 },
    { "zmm20h",  32, 0 },
    { "zmm21h",  32, 0 },
    { "zmm22h",  32, 0 },
    { "zmm23h",  32, 0 },
    { "zmm24h",  32, 0 },
    { "zmm25h",  32, 0 },
    { "zmm26h",  32, 0 },
    { "zmm27h",  32, 0 },
    { "zmm28h",  32, 0 },
    { "zmm29h",  32, 0 },
    { "zmm30h",  32, 0 },
    { "zmm31h",  32, 0 },
};

void gdb_init()
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
    printk("gdbstub reply: \"%*.*s\"\n", int(size), int(size), data);

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

        GDBSTUB_TRACE("Sending reply\n");
        port->write(reply_buf, reply_index, reply_index);

        GDBSTUB_TRACE("Waiting for ACK\n");
        port->read(&ack, 1, 1);

        if (ack != '+')
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
                              " in IDLE state: %#x '%c'\n", uint8_t(ch),
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
                printk("gdbstub command: %s\n", cmd_buf);
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

    GDBSTUB_TRACE("Opening serial port\n");

    port = uart_dev_t::open(0x3f8, 4, 115200, 8, 'N', 1, false);

    port->route_irq(gdb_cpu_ctrl_t::get_gdb_cpu());

    c_cpu = 1;
    g_cpu = 1;
    q_cpu = 1;

    init_reg_offsets();

    isr_context_t *ctx;
    gdb_signal_idx_t sig;

    for (;;) {
        ssize_t rcvd = port->read(rx_buf, MAX_BUFFER_SIZE, 0);

        if (rcvd != 0) {
            GDBSTUB_TRACE("Received serial data:\n");
            hex_dump(rx_buf, rcvd);

            data_received(rx_buf, rcvd);
        } else {
            // Poll for a frozen CPU
            int cpu_nr;

            if (run_state == run_state_t::STEPPING)
                cpu_nr = step_cpu_nr;
            else
                cpu_nr = gdb_cpu_ctrl_t::is_cpu_frozen(-1);

            if (run_state == run_state_t::STOPPED || cpu_nr == 0 ||
                    !gdb_cpu_ctrl_t::is_cpu_frozen(cpu_nr)) {
                halt();
                continue;
            }

            if (run_state == run_state_t::RUNNING)
                gdb_cpu_ctrl_t::freeze_all(c_cpu);

            run_state = run_state_t::STOPPED;
            step_cpu_nr = 0;

            ctx = gdb_cpu_ctrl_t::context_of(cpu_nr);
            sig = gdb_cpu_ctrl_t::signal_from_intr(ISR_CTX_INTR(ctx));

            if (ISR_CTX_INTR(ctx) == INTR_EX_BREAKPOINT) {
                // If we planted the breakpoint, adjust RIP
                if (gdb_cpu_ctrl_t::breakpoint_get_byte(
                            (uint8_t*)ISR_CTX_REG_RIP(ctx) - 1,
                            ctx->gpr.cr3) >= 0) {
                    ISR_CTX_REG_RIP(ctx) = (int(*)(void*))
                            ((uint8_t*)ISR_CTX_REG_RIP(ctx) - 1);
                }

                //replyf("T%02xswbreak:;", sig);
            }
            //else {
                replyf("T%02xthread:%x;", sig, cpu_nr);
            //}
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

bool gdbstub_t::parse_memop(uintptr_t& addr, size_t& size, char const *&input)
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
    size_t volatile memop_index = 0;
    uint8_t const *memop_ptr;
    isr_context_t const *ctx;

    if (!parse_memop(memop_addr, memop_size, input))
        return reply("E02");

    memop_ptr = (uint8_t *)memop_addr;

    ctx = gdb_cpu_ctrl_t::context_of(g_cpu);
    saved_cr3 = cpu_page_directory_get();

    __try {
        cpu_page_directory_set(ctx->gpr.cr3);
        cpu_tlb_flush();
        __try {
            for (memop_index = 0; memop_index < memop_size; ++memop_index) {
                size_t index = memop_index;

                uint8_t const& mem_value = memop_ptr[index];

                // Read the old byte if a software breakpoint is placed there
                int bp_byte = gdb_cpu_ctrl_t::breakpoint_get_byte(
                            &mem_value, ctx->gpr.cr3);

                // Read memory or use saved value from software breakpoint
                if (bp_byte < 0)
                    to_hex_bytes(tx_buf + index*2, mem_value);
                else
                    to_hex_bytes(tx_buf + index*2, uint8_t(bp_byte));
            }
        }
        __catch {
        }
    }
    __catch {
    }

    cpu_page_directory_set(saved_cr3);
    cpu_tlb_flush();

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
    saved_cr3 = cpu_page_directory_get();

    ok = false;

    __try {
        cpu_page_directory_set(ctx->gpr.cr3);
        cpu_tlb_flush();
        __try {
            for (memop_index = 0; memop_index < memop_size; ++memop_index) {
                uint8_t& mem_value = memop_ptr[memop_index];
                uint8_t new_value;
                from_hex<uint8_t>(&new_value, input);
                if (!gdb_cpu_ctrl_t::breakpoint_set_byte(
                            &mem_value, ctx->gpr.cr3, new_value))
                    mem_value = new_value;
            }
            ok = true;
        }
        __catch {
        }
    }
    __catch {
    }

    cpu_page_directory_set(saved_cr3);
    cpu_tlb_flush();

    return reply(ok ? "OK" : "E14");
}

bool gdbstub_t::get_target_desc(
        unique_ptr<char[]>& result, size_t& result_sz,
        char const *annex, size_t annex_sz)
{
    // The following XML target descriptions were copied from the GDB
    // source, with the following copyright notice:
    //
    // Copyright (C) 2010-2017 Free Software Foundation, Inc.
    // Copying and distribution of this file, with or without
    // modification, are permitted in any medium without royalty
    // provided the copyright notice and this notice are
    // preserved.

    static constexpr int x86_64_core_bits =
            16*64 +         // General registers
            64 +            // rip
            7*32 +          // flags and segments
            8*80 + 8*32 +   // x87 fpu
            16*128 +        // xmm0-xmm15 127:0
            32 +            // mxcsr
            2*64;           // fsbase, gsbase

    static constexpr int x86_64_avx_bits =
            16*128;         // ymm0-ymm15 255:128

    static constexpr int x86_64_avx512_bits =
            16*128 +        // xmm16-xmm31 127:0
            16*128 +        // ymm16-ymm31 255:128
            8*64 +          // k0-k7
            32*256;         // zmm0-zmm31 511:256

    static constexpr char const x86_64_target_header[] =
        R"*(<?xml version="1.0"?>)*"
        R"*(<!DOCTYPE feature SYSTEM "gdb-target.dtd">)*"
        R"*(<target>)*"
        R"*(<architecture>i386:x86-64</architecture>)*";

    static constexpr char const x86_64_core[] =
        R"*(<feature name="org.gnu.gdb.i386.core">)*"
        R"*(<flags id="i386_eflags" size="4">)*"
        R"*(<field name="CF" start="0" end="0"/>)*"
        R"*(<field name="" start="1" end="1"/>)*"
        R"*(<field name="PF" start="2" end="2"/>)*"
        R"*(<field name="AF" start="4" end="4"/>)*"
        R"*(<field name="ZF" start="6" end="6"/>)*"
        R"*(<field name="SF" start="7" end="7"/>)*"
        R"*(<field name="TF" start="8" end="8"/>)*"
        R"*(<field name="IF" start="9" end="9"/>)*"
        R"*(<field name="DF" start="10" end="10"/>)*"
        R"*(<field name="OF" start="11" end="11"/>)*"
        R"*(<field name="NT" start="14" end="14"/>)*"
        R"*(<field name="RF" start="16" end="16"/>)*"
        R"*(<field name="VM" start="17" end="17"/>)*"
        R"*(<field name="AC" start="18" end="18"/>)*"
        R"*(<field name="VIF" start="19" end="19"/>)*"
        R"*(<field name="VIP" start="20" end="20"/>)*"
        R"*(<field name="ID" start="21" end="21"/>)*"
        R"*(</flags>)*"

        // 16*64
        R"*(<reg name="rax" bitsize="64" type="int64"/>)*"
        R"*(<reg name="rbx" bitsize="64" type="int64"/>)*"
        R"*(<reg name="rcx" bitsize="64" type="int64"/>)*"
        R"*(<reg name="rdx" bitsize="64" type="int64"/>)*"
        R"*(<reg name="rsi" bitsize="64" type="int64"/>)*"
        R"*(<reg name="rdi" bitsize="64" type="int64"/>)*"
        R"*(<reg name="rbp" bitsize="64" type="data_ptr"/>)*"
        R"*(<reg name="rsp" bitsize="64" type="data_ptr"/>)*"
        R"*(<reg name="r8" bitsize="64" type="int64"/>)*"
        R"*(<reg name="r9" bitsize="64" type="int64"/>)*"
        R"*(<reg name="r10" bitsize="64" type="int64"/>)*"
        R"*(<reg name="r11" bitsize="64" type="int64"/>)*"
        R"*(<reg name="r12" bitsize="64" type="int64"/>)*"
        R"*(<reg name="r13" bitsize="64" type="int64"/>)*"
        R"*(<reg name="r14" bitsize="64" type="int64"/>)*"
        R"*(<reg name="r15" bitsize="64" type="int64"/>)*"

        // 64 + 7*32
        R"*(<reg name="rip" bitsize="64" type="code_ptr"/>)*"
        R"*(<reg name="eflags" bitsize="32" type="i386_eflags"/>)*"
        R"*(<reg name="cs" bitsize="32" type="int32"/>)*"
        R"*(<reg name="ss" bitsize="32" type="int32"/>)*"
        R"*(<reg name="ds" bitsize="32" type="int32"/>)*"
        R"*(<reg name="es" bitsize="32" type="int32"/>)*"
        R"*(<reg name="fs" bitsize="32" type="int32"/>)*"
        R"*(<reg name="gs" bitsize="32" type="int32"/>)*"

        // 8*80
        R"*(<reg name="st0" bitsize="80" type="i387_ext"/>)*"
        R"*(<reg name="st1" bitsize="80" type="i387_ext"/>)*"
        R"*(<reg name="st2" bitsize="80" type="i387_ext"/>)*"
        R"*(<reg name="st3" bitsize="80" type="i387_ext"/>)*"
        R"*(<reg name="st4" bitsize="80" type="i387_ext"/>)*"
        R"*(<reg name="st5" bitsize="80" type="i387_ext"/>)*"
        R"*(<reg name="st6" bitsize="80" type="i387_ext"/>)*"
        R"*(<reg name="st7" bitsize="80" type="i387_ext"/>)*"

        // 8*32
        R"*(<reg name="fctrl" bitsize="32" type="int" group="float"/>)*"
        R"*(<reg name="fstat" bitsize="32" type="int" group="float"/>)*"
        R"*(<reg name="ftag" bitsize="32" type="int" group="float"/>)*"
        R"*(<reg name="fiseg" bitsize="32" type="int" group="float"/>)*"
        R"*(<reg name="fioff" bitsize="32" type="int" group="float"/>)*"
        R"*(<reg name="foseg" bitsize="32" type="int" group="float"/>)*"
        R"*(<reg name="fooff" bitsize="32" type="int" group="float"/>)*"
        R"*(<reg name="fop" bitsize="32" type="int" group="float"/>)*"
        R"*(</feature>)*";

    static constexpr char const x86_64_sse[] =
        R"*(<feature name="org.gnu.gdb.i386.sse">)*"
        R"*(<vector id="v4f" type="ieee_single" count="4"/>)*"
        R"*(<vector id="v2d" type="ieee_double" count="2"/>)*"
        R"*(<vector id="v16i8" type="int8" count="16"/>)*"
        R"*(<vector id="v8i16" type="int16" count="8"/>)*"
        R"*(<vector id="v4i32" type="int32" count="4"/>)*"
        R"*(<vector id="v2i64" type="int64" count="2"/>)*"
        R"*(<union id="vec128">)*"
        R"*(<field name="v4_float" type="v4f"/>)*"
        R"*(<field name="v2_double" type="v2d"/>)*"
        R"*(<field name="v16_int8" type="v16i8"/>)*"
        R"*(<field name="v8_int16" type="v8i16"/>)*"
        R"*(<field name="v4_int32" type="v4i32"/>)*"
        R"*(<field name="v2_int64" type="v2i64"/>)*"
        R"*(<field name="uint128" type="uint128"/>)*"
        R"*(</union>)*"
        R"*(<flags id="i386_mxcsr" size="4">)*"
        R"*(<field name="IE" start="0" end="0"/>)*"
        R"*(<field name="DE" start="1" end="1"/>)*"
        R"*(<field name="ZE" start="2" end="2"/>)*"
        R"*(<field name="OE" start="3" end="3"/>)*"
        R"*(<field name="UE" start="4" end="4"/>)*"
        R"*(<field name="PE" start="5" end="5"/>)*"
        R"*(<field name="DAZ" start="6" end="6"/>)*"
        R"*(<field name="IM" start="7" end="7"/>)*"
        R"*(<field name="DM" start="8" end="8"/>)*"
        R"*(<field name="ZM" start="9" end="9"/>)*"
        R"*(<field name="OM" start="10" end="10"/>)*"
        R"*(<field name="UM" start="11" end="11"/>)*"
        R"*(<field name="PM" start="12" end="12"/>)*"
        R"*(<field name="FZ" start="15" end="15"/>)*"
        R"*(</flags>)*"

        // 16*128
        R"*(<reg name="xmm0" bitsize="128" type="vec128" regnum="40"/>)*"
        R"*(<reg name="xmm1" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm2" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm3" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm4" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm5" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm6" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm7" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm8" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm9" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm10" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm11" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm12" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm13" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm14" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm15" bitsize="128" type="vec128"/>)*"

        // 1*32
        R"*(<reg name="mxcsr" bitsize="32" type="i386_mxcsr")*"
        R"*( group="vector"/>)*"
        R"*(</feature>)*";

    static constexpr char const x86_64_segments[] =
        // 2*64
        R"*(<feature name="org.gnu.gdb.i386.segments">)*"
        R"*(<reg name="fs_base" bitsize="64" type="int"/>)*"
        R"*(<reg name="gs_base" bitsize="64" type="int"/>)*"
        R"*(</feature>)*";

    static constexpr char const x86_64_avx[] =
        R"*(<feature name="org.gnu.gdb.i386.avx">)*"
        // 16*128
        R"*(<reg name="ymm0h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm1h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm2h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm3h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm4h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm5h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm6h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm7h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm8h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm9h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm10h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm11h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm12h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm13h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm14h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm15h" bitsize="128" type="uint128"/>)*"
        R"*(</feature>)*";

    static constexpr char const x86_64_avx512[] =
        R"*(<feature name="org.gnu.gdb.i386.avx512">)*"
        R"*(<vector id="v4f" type="ieee_single" count="4"/>)*"
        R"*(<vector id="v2d" type="ieee_double" count="2"/>)*"
        R"*(<vector id="v16i8" type="int8" count="16"/>)*"
        R"*(<vector id="v8i16" type="int16" count="8"/>)*"
        R"*(<vector id="v4i32" type="int32" count="4"/>)*"
        R"*(<vector id="v2i64" type="int64" count="2"/>)*"

        R"*(<union id="vec128">)*"
        R"*(<field name="v4_float" type="v4f"/>)*"
        R"*(<field name="v2_double" type="v2d"/>)*"
        R"*(<field name="v16_int8" type="v16i8"/>)*"
        R"*(<field name="v8_int16" type="v8i16"/>)*"
        R"*(<field name="v4_int32" type="v4i32"/>)*"
        R"*(<field name="v2_int64" type="v2i64"/>)*"
        R"*(<field name="uint128" type="uint128"/>)*"
        R"*(</union>)*"

        // 16*128
        R"*(<reg name="xmm16" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm17" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm18" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm19" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm20" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm21" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm22" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm23" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm24" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm25" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm26" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm27" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm28" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm29" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm30" bitsize="128" type="vec128"/>)*"
        R"*(<reg name="xmm31" bitsize="128" type="vec128"/>)*"

        // 16*128
        R"*(<reg name="ymm16h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm17h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm18h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm19h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm20h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm21h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm22h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm23h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm24h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm25h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm26h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm27h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm28h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm29h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm30h" bitsize="128" type="uint128"/>)*"
        R"*(<reg name="ymm31h" bitsize="128" type="uint128"/>)*"

        R"*(<vector id="v2ui128" type="uint128" count="2"/>)*"

        // 8*64
        R"*(<reg name="k0" bitsize="64" type="uint64"/>)*"
        R"*(<reg name="k1" bitsize="64" type="uint64"/>)*"
        R"*(<reg name="k2" bitsize="64" type="uint64"/>)*"
        R"*(<reg name="k3" bitsize="64" type="uint64"/>)*"
        R"*(<reg name="k4" bitsize="64" type="uint64"/>)*"
        R"*(<reg name="k5" bitsize="64" type="uint64"/>)*"
        R"*(<reg name="k6" bitsize="64" type="uint64"/>)*"
        R"*(<reg name="k7" bitsize="64" type="uint64"/>)*"

        // 32*256
        R"*(<reg name="zmm0h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm1h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm2h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm3h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm4h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm5h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm6h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm7h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm8h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm9h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm10h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm11h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm12h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm13h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm14h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm15h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm16h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm17h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm18h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm19h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm20h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm21h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm22h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm23h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm24h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm25h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm26h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm27h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm28h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm29h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm30h" bitsize="256" type="v2ui128"/>)*"
        R"*(<reg name="zmm31h" bitsize="256" type="v2ui128"/>)*"
        R"*(</feature>)*";

    static constexpr char const x86_64_target_footer[] =
        R"*(</target>)*";

    if (annex_sz == 10 && strncmp(annex, "target.xml", annex_sz) != 0)
        return false;

    result_sz = (countof(x86_64_target_header) - 1);

    int bits = x86_64_core_bits;

    result_sz += (countof(x86_64_core) - 1);
    result_sz += (countof(x86_64_sse) - 1);
    result_sz += (countof(x86_64_segments) - 1);

    if (GDBSTUB_FORCE_FULL_CTX || sse_avx_offset) {
        result_sz += (countof(x86_64_avx) - 1);
        bits += x86_64_avx_bits;
    }

    if (GDBSTUB_FORCE_FULL_CTX || sse_avx512_upper_offset) {
        result_sz += (countof(x86_64_avx512) - 1);
        bits += x86_64_avx512_bits;
    }

    result_sz += (countof(x86_64_target_footer) - 1);

    result.reset(new char[result_sz + 1]);

    char *output = result;

    output = stpcpy(output, x86_64_target_header);
    output = stpcpy(output, x86_64_core);
    output = stpcpy(output, x86_64_sse);
    output = stpcpy(output, x86_64_segments);

    if (GDBSTUB_FORCE_FULL_CTX || sse_avx_offset)
        output = stpcpy(output, x86_64_avx);

    if (GDBSTUB_FORCE_FULL_CTX || sse_avx512_upper_offset)
        output = stpcpy(output, x86_64_avx512);

    output = stpcpy(output, x86_64_target_footer);

    // 4 bits per encoded hex digit
    encoded_ctx_sz = bits >> 2;

    size_t len_chk = strlen(result);

    assert(len_chk == result_sz);

    return true;
}

gdbstub_t::rx_state_t gdbstub_t::handle_query_features(char const *input)
{
    char const *annex_st = input;
    char const *annex_en = strchr(annex_st, ':');

    if (!annex_en)
        return reply("E03");

    unique_ptr<char[]> desc;
    size_t desc_sz = 0;
    bool exists = get_target_desc(desc, desc_sz, annex_st, annex_en - annex_st);

    if (!exists)
        return reply("E00");

    input = annex_en + 1;

    size_t offset = from_hex<size_t>(&input);

    if (*input++ != ',')
        return reply("E01");

    size_t length = from_hex<size_t>(&input);

    // Allow room for $m#xx envelope
    if (length > 5)
        length -= 5;

    char prefix;
    size_t reply_len = 0;
    if (offset >= desc_sz) {
        // Past end, return empty "last" packet reply
        return reply("l");
    } else if (desc_sz - offset <= length) {
        reply_len = desc_sz - offset;
        prefix = 'l';
    } else {
        reply_len = length;
        prefix = 'm';
    }

    tx_buf[0] = prefix;
    memcpy(tx_buf + 1, &desc[offset], reply_len);

    return reply(tx_buf, reply_len + 1);
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

    switch (ch) {
    case 3:
        // Ctrl-c pressed on client
        GDBSTUB_TRACE("Got interrupt request\n");
        step_cpu_nr = c_cpu;
        run_state = run_state_t::STEPPING;
        gdb_cpu_ctrl_t::freeze_all(c_cpu);
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
            set_context(ctx, input, cmd_index);
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
        set_context(ctx, tx_buf, cmd_index);
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
            // Query for feature support
            return replyf("PacketSize=%x;qXfer:features:read+",
                          MAX_BUFFER_SIZE);
        } else if (match(input, "Xfer:features:read", ":")) {
            // Get machine description
            return handle_query_features(input);
        } else if (!strcmp(input, "fThreadInfo") ||
                !strcmp(input, "sThreadInfo")) {
            // Enumerate threads
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
            // Get human readable description for thread
            input += 16;
            thread = from_hex<int>(&input);
            bool cpu_running = gdb_cpu_ctrl_t::is_cpu_running(thread);
            return replyf_hex("CPU#%d [%s]", thread,
                              cpu_running ? "running" : "halted");
        } else if (!strncmp(input, "Attached", 8)) {
            return reply("1");
        } else if (!strncmp(input, "Symbol::", 8)) {
            // Can request symbol lookups here, for now say we don't want any
            return reply("OK");
        }
        break;

    case 'T':
        thread = from_hex<int>(&input);

        if (thread >= 1 && thread <= gdb_cpu_ctrl_t::get_gdb_cpu())
            return reply("OK");

        return reply("E01");

    case 'v':
        if (!strcmp(input, "Cont?")) {
            // "vCont?" command
            return reply("vCont;s;c;S;C;r");
        } else if (match(input, "Cont", ":;")) {
            // Initialize an array with one entry per CPU
            // Values are set once, only entries with 0 value are modified

            vector<step_action_t> step_actions(gdb_cpu_ctrl_t::get_gdb_cpu());

            step_cpu_nr = 0;

            while (*input) {
                step_action_t action{};
                char sep = 0;

                if (match(input, "c", ":", &sep))
                    action.type = step_action_t::CONT;
                else if (match(input, "s", ":", &sep))
                    action.type = step_action_t::STEP;
                else if (*input == 'r') {
                    ++input;
                    while (*input && *input == ' ')
                        ++input;
                    from_hex<uintptr_t>(&action.start, input);
                    if (*input != ',')
                        break;
                    ++input;
                    from_hex<uintptr_t>(&action.end, input);

                    if (*input == ':')
                        ++input;

                    if (action.end > action.start)
                        action.type = step_action_t::RANGE;
                    else
                        action.type = step_action_t::STEP;
                } else {
                    break;
                }

                thread = 0;
                if (*input)
                    thread = from_hex<int>(&input);

                if (thread > 0) {
                    // Step a specific CPU
                    if (step_actions.at(thread - 1).type ==
                            step_action_t::NONE) {
                        step_actions[thread - 1] = action;

                        if (action.type == step_action_t::STEP &&
                                step_cpu_nr == 0)
                            step_cpu_nr = thread;
                    }
                } else {
                    // Step all CPUs that don't have an action assigned
                    for (step_action_t& cpu_action : step_actions) {
                        if (cpu_action.type == step_action_t::NONE)
                            cpu_action = action;
                    }
                }

                run_state = step_cpu_nr
                        ? run_state_t::STEPPING
                        : run_state_t::RUNNING;

                // Do all of the continues (if any)
                int wake = 1;
                for (step_action_t& cpu_action : step_actions) {
                    if (cpu_action.type == step_action_t::CONT) {
                        gdb_cpu_ctrl_t::set_step_range(wake, 0, 0);
                        gdb_cpu_ctrl_t::continue_frozen(wake, false);
                    }
                    ++wake;
                }

                // Do all of the range steps (if any)
                wake = 1;
                for (step_action_t& cpu_action : step_actions) {
                    if (cpu_action.type == step_action_t::RANGE) {
                        gdb_cpu_ctrl_t::set_step_range(
                                    wake, cpu_action.start, cpu_action.end);
                        gdb_cpu_ctrl_t::continue_frozen(wake, true);
                    }
                    ++wake;
                }

                // Do all of the steps (if any)
                wake = 1;
                for (step_action_t& cpu_action : step_actions) {
                    if (cpu_action.type == step_action_t::STEP) {
                        gdb_cpu_ctrl_t::set_step_range(wake, 0, 0);
                        gdb_cpu_ctrl_t::continue_frozen(wake, true);
                    }
                    ++wake;
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

        if (add) {
            if (!gdb_cpu_ctrl_t::breakpoint_add(
                        type, addr, ctx->gpr.cr3, kind)) {
                // Breakpoint could not be added
                return reply("E01");
            }
        } else {
            gdb_cpu_ctrl_t::breakpoint_del(type, addr, ctx->gpr.cr3, kind);
        }

        if (type == gdb_breakpoint_type_t::HARDWARE)
            gdb_cpu_ctrl_t::sync_hw_bp();

        return reply("OK");

    case 'k':
        // FIXME: reboot the machine / kill the vm
        break;
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

template<typename T>
void gdbstub_t::append_ctx_reply(char *reply, int& ofs, T *value_ptr)
{
    if (uintptr_t(value_ptr) > 0x400000) {
        ofs += to_hex_bytes(reply + ofs, *value_ptr);
    } else {
        memset(reply + ofs, 'x', sizeof(T)*2);
        ofs += sizeof(T)*2;
    }
}

size_t gdbstub_t::get_context(char *reply, const isr_context_t *ctx)
{
    int ofs = 0;

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

    bool has_fpu_ctx = ISR_CTX_FPU(ctx);
    uint32_t temp;

    for (size_t st = 0; st < 8; ++st) {
        temp = has_fpu_ctx ? ISR_CTX_FPU_STn_31_0(ctx, st) : 0;
        append_ctx_reply<uint32_t>(reply, ofs, has_fpu_ctx ? &temp : nullptr);

        temp = has_fpu_ctx ? ISR_CTX_FPU_STn_63_32(ctx, st) : 0;
        append_ctx_reply<uint32_t>(reply, ofs, has_fpu_ctx ? &temp : nullptr);

        temp = has_fpu_ctx ? ISR_CTX_FPU_STn_79_64(ctx, st) : 0;
        append_ctx_reply<uint16_t>(reply, ofs,
                                   has_fpu_ctx ? (uint16_t*)&temp : nullptr);
    }

    temp = has_fpu_ctx ? ISR_CTX_FPU_FCW(ctx) : 0;
    append_ctx_reply<uint32_t>(reply, ofs, has_fpu_ctx ? &temp : nullptr);

    temp = has_fpu_ctx ? ISR_CTX_FPU_FSW(ctx) : 0;
    append_ctx_reply<uint32_t>(reply, ofs, has_fpu_ctx ? &temp : nullptr);

    temp = has_fpu_ctx ? ISR_CTX_FPU_FTW(ctx) : 0;
    append_ctx_reply<uint32_t>(reply, ofs, has_fpu_ctx ? &temp : nullptr);

    temp = has_fpu_ctx ? ISR_CTX_FPU_FIS(ctx) : 0;
    append_ctx_reply<uint32_t>(reply, ofs, has_fpu_ctx ? &temp : nullptr);

    temp = has_fpu_ctx ? ISR_CTX_FPU_FIP(ctx) : 0;
    append_ctx_reply<uint32_t>(reply, ofs, has_fpu_ctx ? &temp : nullptr);

    temp = has_fpu_ctx ? ISR_CTX_FPU_FDS(ctx) : 0;
    append_ctx_reply<uint32_t>(reply, ofs, has_fpu_ctx ? &temp : nullptr);

    temp = has_fpu_ctx ? ISR_CTX_FPU_FDP(ctx) : 0;
    append_ctx_reply<uint32_t>(reply, ofs, has_fpu_ctx ? &temp : nullptr);

    temp = has_fpu_ctx ? ISR_CTX_FPU_FOP(ctx) : 0;
    append_ctx_reply<uint32_t>(reply, ofs, has_fpu_ctx ? &temp : nullptr);

    for (size_t xmm = 0; xmm < 16; ++xmm) {
        for (size_t i = 0; i < 2; ++i) {
            append_ctx_reply(reply, ofs, &ISR_CTX_SSE_XMMn_q(ctx, xmm, i));
        }
    }

    append_ctx_reply(reply, ofs, &ISR_CTX_SSE_MXCSR(ctx));

    // fsbase
    memset(reply + ofs, 'x', 16);
    ofs += 16;

    // gsbase
    memset(reply + ofs, 'x', 16);
    ofs += 16;

    if (GDBSTUB_FORCE_FULL_CTX || sse_avx_offset) {
        // ymm0-ymm15 255:128
        memset(reply + ofs, 'x', 16*16*2);
        ofs += 16*16*2;
    }

    if (GDBSTUB_FORCE_FULL_CTX || sse_avx512_upper_offset) {
        // xmm16-xmm31 127:0
        memset(reply + ofs, 'x', 16*16*2);
        ofs += 16*16*2;

        // ymm16-ymm31 127:0
        memset(reply + ofs, 'x', 16*16*2);
        ofs += 16*16*2;

        // k0-k7
        memset(reply + ofs, 'x', 8*8*2);
        ofs += 8*8*2;

        // zmm0-zmm31 511:256
        memset(reply + ofs, 'x', 32*32*2);
        ofs += 32*32*2;
    }

    reply[ofs] = 0;

    assert(ofs == encoded_ctx_sz);

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

size_t gdbstub_t::set_context(isr_context_t *ctx,
                              char const *data, size_t data_len)
{
    char const *input = data;
    char const *end = data + data_len;

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

    bool has_fpu_ctx = ISR_CTX_FPU(ctx);

    for (size_t st = 0; st < 8; ++st) {
        if (has_fpu_ctx) {
            from_hex_bytes(&ISR_CTX_FPU_STn_31_0(ctx, st), input);
            from_hex_bytes(&ISR_CTX_FPU_STn_63_32(ctx, st), input);
            from_hex_bytes(&ISR_CTX_FPU_STn_79_64(ctx, st), input);
        } else {
            input += 20;
        }
    }

    if (has_fpu_ctx) {
        from_hex_bytes(&ISR_CTX_FPU_FCW(ctx), input, sizeof(uint32_t));
        from_hex_bytes(&ISR_CTX_FPU_FSW(ctx), input, sizeof(uint32_t));
        from_hex_bytes(&ISR_CTX_FPU_FTW(ctx), input, sizeof(uint32_t));
        from_hex_bytes(&ISR_CTX_FPU_FIS(ctx), input, sizeof(uint32_t));
        from_hex_bytes(&ISR_CTX_FPU_FIP(ctx), input, sizeof(uint32_t));
        from_hex_bytes(&ISR_CTX_FPU_FDS(ctx), input, sizeof(uint32_t));
        from_hex_bytes(&ISR_CTX_FPU_FDP(ctx), input, sizeof(uint32_t));
        from_hex_bytes(&ISR_CTX_FPU_FOP(ctx), input, sizeof(uint32_t));
    } else {
        input += 8 * 8;
    }

    for (size_t xmm = 0; xmm < 16; ++xmm) {
        for (size_t i = 0; i < 2; ++i) {
            from_hex_bytes(&ISR_CTX_SSE_XMMn_q(ctx, xmm, i), input);
        }
    }

    from_hex_bytes(&ISR_CTX_SSE_MXCSR(ctx), input);

    // Skip prev_rax
    if (input + 16 <= end)
        input += 16;

    // Skip fs_base
    if (input + 16 <= end)
        input += 16;

    // Skip gs_base
    if (input + 16 <= end)
        input += 16;

    if (GDBSTUB_FORCE_FULL_CTX || sse_avx_offset) {
        // ymm0-ymm15 255:128
        input += 16*16*2;
    }

    if (GDBSTUB_FORCE_FULL_CTX || sse_avx512_upper_offset) {
        // xmm16-xmm31 127:0
        input += 16*16*2;

        // ymm16-ymm31 127:0
        input += 16*16*2;

        // k0-k7
        input += 8*8*2;

        // zmm0-zmm31 511:256
        input += 32*64*2;
    }

    assert(input - data == encoded_ctx_sz);

    return input - data;
}

gdb_cpu_ctrl_t::bp_list::iterator gdb_cpu_ctrl_t::breakpoint_find(
        bp_list &list, uintptr_t addr, uintptr_t page_dir, uint8_t kind)
{
    return find_if(list.begin(), list.end(), [&](breakpoint_t const& item) {
        return item.addr == addr &&
                (!kind || item.kind == kind) &&
                (!page_dir || item.page_dir == page_dir);
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
    if (!mpresent(addr, sizeof(T)))
        return false;

    uintptr_t orig_pagedir = cpu_page_directory_get();
    GDBSTUB_TRACE("Switching from stub pagedir (%zx)"
                  " to target pagedir (%zx)\n", orig_pagedir, page_dir);

    cpu_page_directory_set(page_dir);
    cpu_tlb_flush();
    cpu_cr0_change_bits(CPU_CR0_WP, 0);
    *old_value = atomic_xchg((uint8_t*)addr, value);
    GDBSTUB_TRACE("Wrote %#lx to %zx, replaced %#" PRIx64 "\n",
                  uintptr_t(value), addr, uintptr_t(*old_value));
    cpu_cr0_change_bits(0, CPU_CR0_WP);
    cpu_page_directory_set(orig_pagedir);
    cpu_tlb_flush();
    GDBSTUB_TRACE("Switched back to stub pagedir (%zx)\n", orig_pagedir);

    return true;
}

bool gdb_cpu_ctrl_t::breakpoint_add(
        gdb_breakpoint_type_t type, uintptr_t addr,
        uintptr_t page_dir, uint8_t size)
{
    // See if there are too many breakpoints
    if (type != gdb_breakpoint_type_t::SOFTWARE && instance.bp_hw.size() >=
            X86_MAX_HW_BP)
        return false;

    auto& list = instance.breakpoint_list(type);
    auto it = instance.breakpoint_find(list, addr, page_dir, size);

    // Adding a duplicate breakpoint should be idempotent
    if (it != list.end())
        return true;

    list.emplace_back(type, addr, page_dir, size, 0, false);

    // If it is a software breakpoint, and it didn't apply,
    // then remove it and report failure to caller
    if (type == gdb_breakpoint_type_t::SOFTWARE &&
            !instance.breakpoint_toggle(list.back(), true)) {
        list.pop_back();
        return false;
    }

    return true;
}

bool gdb_cpu_ctrl_t::breakpoint_del(gdb_breakpoint_type_t type,
                                    uintptr_t addr, uintptr_t page_dir,
                                    uint8_t kind)
{
    auto& list = instance.breakpoint_list(type);
    auto it = instance.breakpoint_find(list, addr, page_dir, kind);
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

        bp.active = true;
    } else if (!activate && bp.active) {
        if (!breakpoint_write_target(bp.addr, bp.orig_data,
                                     &old_value, bp.page_dir))
            return false;

        bp.active = false;
        assert(old_value == X86_BREAKPOINT_OPCODE);
    }
    return true;
}

void gdb_cpu_ctrl_t::breakpoint_toggle_all(bool activate)
{
    instance.breakpoint_toggle_list(instance.bp_sw, activate);
    instance.breakpoint_toggle_list(instance.bp_hw, activate);
}

int gdb_cpu_ctrl_t::breakpoint_get_byte(uint8_t const *addr, uintptr_t page_dir)
{
    auto it = instance.breakpoint_find(instance.bp_sw, uintptr_t(addr),
                                       page_dir, 0);

    if (it == instance.bp_sw.end() || !it->active)
        return -1;

    return it->orig_data;
}

bool gdb_cpu_ctrl_t::breakpoint_set_byte(uint8_t *addr, uintptr_t page_dir,
                                         uint8_t value)
{
    auto it = instance.breakpoint_find(instance.bp_sw, uintptr_t(addr),
                                       page_dir, 0);

    if (it == instance.bp_sw.end() || !it->active)
        return false;

    it->orig_data = value;

    return true;
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
        // Specific CPU
        gdb_cpu_t const *cpu = instance.cpu_from_nr(cpu_nr);
        return (cpu && cpu->state == gdb_cpu_state_t::FROZEN)
                ? cpu->cpu_nr
                : 0;
    }

    // Any CPU
    for (gdb_cpu_t& cpu : instance.cpus) {
        if (cpu.state == gdb_cpu_state_t::FROZEN)
            return cpu.cpu_nr;
    }

    return 0;
}

void gdb_cpu_ctrl_t::freeze_all(int first)
{
    gdb_cpu_t* first_cpu = nullptr;

    if (first > 0)
        first_cpu = instance.cpu_from_nr(first);

    if (first_cpu)
        instance.freeze_one(*first_cpu);

    for (auto& cpu : instance.cpus) {
        if (&cpu != first_cpu)
            instance.freeze_one(cpu);
    }
}

void gdb_cpu_ctrl_t::continue_frozen(int cpu_nr, bool single_step)
{
    for (gdb_cpu_t& cpu : instance.cpus) {
        if (cpu_nr > 0 && cpu.cpu_nr != cpu_nr)
            continue;

        if (cpu.state == gdb_cpu_state_t::FROZEN) {
            // Set trap and resume flag if single stepping
            if (single_step)
                ISR_CTX_REG_RFLAGS(cpu.ctx) |= CPU_EFLAGS_TF | CPU_EFLAGS_RF;
            else
                ISR_CTX_REG_RFLAGS(cpu.ctx) &= ~CPU_EFLAGS_TF;

            // Change state to break it out of the halt loop
            cpu.state = gdb_cpu_state_t::RESUMING;

            // Send an NMI to the CPU to wake it up from halt
            GDBSTUB_TRACE("Sending NMI to cpu %d (APICID=%#x)\n",
                          cpu.cpu_nr, cpu.apic_id);
            apic_send_ipi(cpu.apic_id, INTR_EX_NMI);

            // Wait for CPU to pick it up
            cpu_wait_not_value(&cpu.state, gdb_cpu_state_t::RESUMING);
        }
    }
}

void gdb_cpu_ctrl_t::set_step_range(int cpu_nr, uintptr_t st, uintptr_t en)
{
    for (gdb_cpu_t& cpu : instance.cpus) {
        if (cpu_nr > 0 && cpu.cpu_nr != cpu_nr)
            continue;

        cpu.range_step_st = st;
        cpu.range_step_en = en;
    }
}

void gdb_cpu_ctrl_t::sync_hw_bp()
{
    GDBSTUB_TRACE("Synchronizing hardware breakpoints\n");

    auto callback = [] {
        size_t i, e;
        for (i = 0, e = instance.bp_hw.size(); i < X86_MAX_HW_BP; ++i) {
            breakpoint_t& bp = instance.bp_hw[i];

            uintptr_t addr;
            int len;
            int enable;
            int rw;

            if (i < e && i) {
                addr = bp.addr;

                switch (bp.kind) {
                case 1: len = CPU_DR7_LEN_1; break;
                case 2: len = CPU_DR7_LEN_2; break;
                case 4: len = CPU_DR7_LEN_4; break;
                case 8: len = CPU_DR7_LEN_8; break;
                }

                switch (bp.type) {
                case gdb_breakpoint_type_t::HARDWARE:
                    rw = CPU_DR7_RW_INSN;
                    break;

                case gdb_breakpoint_type_t::WRITEWATCH:
                    rw = CPU_DR7_RW_WRITE;
                    break;

                default:
                    assert(!"Invalid hardware breakpoint type,"
                            " treating as ACCESSWATCH");
                    // fall thru...
                case gdb_breakpoint_type_t::READWATCH:
                    // Read watch isn't possible, treat as ACCESSWATCH
                    // fall thru...
                case gdb_breakpoint_type_t::ACCESSWATCH:
                    rw = CPU_DR7_RW_RW;
                    break;
                }

                enable = CPU_DR7_EN_GLOBAL;
            } else {
                addr = 0;
                len = 0;
                enable = 0;
                rw = 0;
            }

            cpu_debug_breakpoint_set_indirect(addr, rw, len, enable, i);
        }
    };

    for (gdb_cpu_t& cpu : instance.cpus) {
        if (cpu.state == gdb_cpu_state_t::FROZEN) {
            cpu.sync_bp = callback;

            GDBSTUB_TRACE("Synchronizing hardware breakpoints"
                          " on CPU %d\n", cpu.cpu_nr);

            // Tell waiting thread to synchronize hardware breakpoints
            cpu.state = gdb_cpu_state_t::SYNC_HW_BP;

            apic_send_ipi(cpu.apic_id, INTR_EX_NMI);

            cpu_wait_value(&cpu.state, gdb_cpu_state_t::FROZEN);
        }
    }
}

void gdb_cpu_ctrl_t::hook_exceptions()
{
    intr_hook(INTR_EX_DEBUG, &gdb_cpu_ctrl_t::exception_handler, "sw_debug");
    intr_hook(INTR_EX_NMI, &gdb_cpu_ctrl_t::exception_handler, "sw_nmi");
    intr_hook(INTR_EX_BREAKPOINT, &gdb_cpu_ctrl_t::exception_handler, "sw_bp");
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
    case INTR_EX_NMI        : return gdb_signal_idx_t::NONE;
    case INTR_EX_BREAKPOINT : return gdb_signal_idx_t::SIGTRAP;
    case INTR_EX_OVF        : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_BOUND      : return gdb_signal_idx_t::NONE;
    case INTR_EX_OPCODE     : return gdb_signal_idx_t::SIGILL;
    case INTR_EX_DEV_NOT_AV : return gdb_signal_idx_t::SIGFPE;
    case INTR_EX_DBLFAULT   : return gdb_signal_idx_t::NONE;
    case INTR_EX_COPR_SEG   : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_TSS        : return gdb_signal_idx_t::NONE;
    case INTR_EX_SEGMENT    : return gdb_signal_idx_t::NONE;
    case INTR_EX_STACK      : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_GPF        : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_PAGE       : return gdb_signal_idx_t::SIGSEGV;
    case INTR_EX_MATH       : return gdb_signal_idx_t::SIGFPE;
    case INTR_EX_ALIGNMENT  : return gdb_signal_idx_t::SIGTRAP;
    case INTR_EX_MACHINE    : return gdb_signal_idx_t::SIGBUS;
    case INTR_EX_SIMD       : return gdb_signal_idx_t::SIGFPE;
    case INTR_EX_VIRTUALIZE : return gdb_signal_idx_t::SIGBUS;
    default                 : return gdb_signal_idx_t::SIGTRAP;
    }
}

char const *gdb_cpu_ctrl_t::signal_name(gdb_signal_idx_t sig)
{
    switch (sig) {
    case gdb_signal_idx_t::NONE:    return "NONE";
    case gdb_signal_idx_t::SIGHUP:  return "SIGHUP";
    case gdb_signal_idx_t::SIGINT:  return "SIGINT";
    case gdb_signal_idx_t::SIGQUIT: return "SIGQUIT";
    case gdb_signal_idx_t::SIGILL:  return "SIGILL";
    case gdb_signal_idx_t::SIGTRAP: return "SIGTRAP";
    case gdb_signal_idx_t::SIGABRT: return "SIGABRT";
    case gdb_signal_idx_t::SIGEMT:  return "SIGEMT";
    case gdb_signal_idx_t::SIGFPE:  return "SIGFPE";
    case gdb_signal_idx_t::SIGKILL: return "SIGKILL";
    case gdb_signal_idx_t::SIGBUS:  return "SIGBUS";
    case gdb_signal_idx_t::SIGSEGV: return "SIGSEGV";
    case gdb_signal_idx_t::SIGSYS:  return "SIGSYS";
    case gdb_signal_idx_t::SIGPIPE: return "SIGPIPE";
    case gdb_signal_idx_t::SIGALRM: return "SIGALRM";
    case gdb_signal_idx_t::SIGTERM: return "SIGTERM";
    default: return "???";
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

    stub_tid = thread_create(gdb_thread, nullptr, 0, false);

    cpu_wait_value(&stub_running, true);

    cpu_irq_disable();
    bool wait = true;
    while (wait)
        thread_yield();
}

void gdb_cpu_ctrl_t::freeze_one(gdb_cpu_t& cpu)
{
    if (cpu.state == gdb_cpu_state_t::RUNNING) {
        cpu.state = gdb_cpu_state_t::FREEZING;

        apic_send_ipi(cpu.apic_id, INTR_EX_NMI);

        cpu_wait_value(&cpu.state, gdb_cpu_state_t::FROZEN);
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

    unique_ptr<gdbstub_t> stub(new gdbstub_t);

    freeze_all(1);

    stub_running = true;
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

void gdb_cpu_ctrl_t::wait(gdb_cpu_t const *cpu)
{
    while (cpu->state == gdb_cpu_state_t::FROZEN)
        halt();
}

isr_context_t *gdb_cpu_ctrl_t::exception_handler(int, isr_context_t *ctx)
{
    return instance.exception_handler(ctx);
}

isr_context_t *gdb_cpu_ctrl_t::exception_handler(isr_context_t *ctx)
{
    gdb_cpu_t *cpu = cpu_from_nr(0);

    if (!cpu) {
        static uintptr_t bp_workaround_addr;

        // This is the GDB stub
        if ((ISR_CTX_REG_RFLAGS(ctx) & CPU_EFLAGS_TF) && bp_workaround_addr) {
            // We are in a single step breakpoint workaround

            // Find the breakpoint we stepped over
            bp_list::iterator it = breakpoint_find(
                        bp_sw, bp_workaround_addr, 0, 0);
            if (it == bp_sw.end())
                return nullptr;

            bp_workaround_addr = 0;

            // Disable single-step
            ISR_CTX_REG_RFLAGS(ctx) &= ~CPU_EFLAGS_TF;

            // Reenable it
            breakpoint_toggle(*it, true);
            return ctx;
        }

        if (ISR_CTX_INTR(ctx) == INTR_EX_BREAKPOINT) {
            // Handle hitting breakpoint in stub by deactivating it,
            // single-stepping stepping, and reenabling it (above)

            // Adjust RIP back to start of instruction
            ISR_CTX_REG_RIP(ctx) = (int(*)(void*))
                    ((char*)ctx->gpr.iret.rip - 1);

            bp_workaround_addr = uintptr_t(ISR_CTX_REG_RIP(ctx));

            // Find the breakpoint
            bp_list::iterator it = breakpoint_find(
                        bp_sw, uintptr_t(ISR_CTX_REG_RIP(ctx)), 0, 0);
            if (it == bp_sw.end())
                return nullptr;

            // Disable it
            breakpoint_toggle(*it, false);

            // Single step the instruction
            ISR_CTX_REG_RFLAGS(ctx) |= CPU_EFLAGS_TF | CPU_EFLAGS_RF;

            return ctx;
        }

        return nullptr;
    }

    // Ignore NMI when not freezing
    if (ISR_CTX_INTR(ctx) == INTR_EX_NMI &&
            cpu->state != gdb_cpu_state_t::FREEZING) {
        GDBSTUB_TRACE("Received NMI on cpu %d, continuing\n", cpu->cpu_nr);
        return ctx;
    }

    // Ignore single step inside step range
    if (ISR_CTX_INTR(ctx) == INTR_EX_DEBUG &&
            uintptr_t(ISR_CTX_REG_RIP(ctx)) >= cpu->range_step_st &&
            uintptr_t(ISR_CTX_REG_RIP(ctx)) < cpu->range_step_en) {
        // Ignore single step inside range
        ISR_CTX_REG_RFLAGS(ctx) |= CPU_EFLAGS_RF | CPU_EFLAGS_TF;
        return ctx;
    }

    cpu->ctx = ctx;

    // GDB thread waits for the state to transition to FROZEN
    cpu->state = gdb_cpu_state_t::FROZEN;

    GDBSTUB_TRACE("CPU entering wait: %d (%s)\n", cpu->cpu_nr,
                  signal_name(signal_from_intr(ISR_CTX_INTR(ctx))));

    while (cpu->state != gdb_cpu_state_t::RESUMING) {
        // Idle the CPU until NMI wakes it
        wait(cpu);

        if (cpu->state == gdb_cpu_state_t::SYNC_HW_BP) {
            cpu->sync_bp();

            cpu->state = gdb_cpu_state_t::FROZEN;
        }
    }

    GDBSTUB_TRACE("CPU woke up: %d\n", cpu->cpu_nr);

    cpu->ctx = nullptr;

    // Only running threads can be transitioned back to FREEZING
    cpu->state = gdb_cpu_state_t::RUNNING;

    // Continue execution with potentially modified context
    return ctx;
}
