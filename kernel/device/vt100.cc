#include "vt100.h"
#include "dev_text.h"
#include "dev_char.h"
#include "keyboard.h"
#include "mouse.h"
#include "cxxstring.h"

class vt100_console_t;

#define S_ESC "\x1b"

class vt100_console_t final : public dev_char_t {
public:


public:
    // dev_char_t interface

    // Reads keyboard input and escape sequence query responses
    errno_t read_async(void *data, int64_t count, iocp_t *iocp) override;

    // Writes display output and applies escape sequences
    errno_t write_async(void *data, int64_t count, iocp_t *iocp) override;

    // Ensure that no write data is buffered, push it out to the screen
    errno_t flush_async(iocp_t *iocp) override;

private:
    struct seq_t {
        char const *seq = nullptr;
        void (vt100_console_t::*handler)(uintptr_t arg) = nullptr;
        uintptr_t arg = 0;
    };

    static seq_t vt100_tab[];

    // Maximum number of CSI integer parameters
    static constexpr const size_t NPAR = 16;

    // ASCII/UTF-8 control characters
    static constexpr const uint8_t NUL = 0;

    // bell (audible alert)
    static constexpr const uint8_t BEL = 0x7;

    // backspace if not at beginning of line
    static constexpr const uint8_t BS  = 0x8;

    // go to next tab stop or end of line if no more tab stops
    static constexpr const uint8_t HT  = 0x9;

    // LF,VT,FF all issue line feed, and if LFNL mode is on, also a CR
    static constexpr const uint8_t LF  = 0xa;
    static constexpr const uint8_t VT  = 0xb;
    static constexpr const uint8_t FF  = 0xc;

    // carriage return
    static constexpr const uint8_t CR  = 0xd;

    // activate G1 character set
    static constexpr const uint8_t SO  = 0xe;

    // activate G0 character set
    static constexpr const uint8_t SI  = 0xf;

    // CAN/SUB interrupt
    static constexpr const uint8_t CAN = 0x18;
    static constexpr const uint8_t SUB = 0x1a;

    // start an escape sequence
    static constexpr const uint8_t ESC = 0x1b;

    // ignored
    static constexpr const uint8_t DEL = 0x7f;

    // control sequence introducer, equivalent to ESC [
    static constexpr const uint8_t CSI = 0x9b;

    enum struct state_t {
        NORMAL,

        // Encountered an escape byte
        IN_ESC,

        // Encountered (ESC, '[') or (CSI)
        IN_CSI,
    };

    state_t state;
    uint8_t utf8_remain;
    char32_t utf8_codepoint;
    std::u32string csi;
    using csi_iter = std::u32string::iterator;

    struct gr_t {
        unsigned font:4;

        // +1 is intense, 0 is normal, -1 is faint
        int intensity:2;
        bool italic:1;
        bool underline:1;

        bool slow_blink:1;
        bool rapid_blink:1;
        bool reverse:1;
        bool conceal:1;

        bool crossed_out:1;
        bool dbl_underline:1;

    };

    template<size_t N>
    int parse_csi_integers(csi_iter&& inp, int (&values)[N])
    {
        char32_t c;
        int n;

        values[0] = 0;

        for (n = 0; inp != csi.end() &&
             ((c = *inp), c >= 0x20 && c < 0x40); ++inp) {
            if (c >= '0' && c <= '9') {
                values[n] *= 10;
                values[n] += c - '0';
            } else if (c == ';') {
                if (unlikely(++n == NPAR))
                    return -1;
                values[n] = 0;
            }
        }

        return n;
    }

    void parse_csi(char32_t final)
    {
        int np;
        int params[NPAR];

        switch (final) {
        case 'A':
            // CUU - cursor up N times, do nothing at edge of screen
            np = parse_csi_integers(csi.begin(), params);

            break;

        case 'B':
            // CUD cursor down N times, do nothing at edge of screen

            break;

        case 'C':
            // CUF cursor forward N times, do nothing at edge of screen
            break;

        case 'D':
            // CUB cursor back N times, do nothing at edge of screen

            break;

        case 'E':
            // CNL cursor beginning of line N lines down

            break;

        case 'F':
            // CPL cursor beginning of line N lines up

            break;

        case 'G':
            // CHA cursor to absolute column N

            break;

        case 'H':
            // CHA cursor to absolute position N;M
            // fall through
        case 'f':
            // HVP horizontal vertical position N;M
            // fall through
            break;

        case 'J':
            // ED erase
            //  (N=0 or missing): cursor to end of screen
            //  (N=1): cursor to beginning of screen,
            //  (N=2): entire screen cursor to top left,
            //  (N=3): entire scrollback cursor to top left

            break;

        case 'K':
            // EL - erase in line
            //  (N=0 or missing): cursor to EOL,
            //  (N=1): cursor to beginning of line,
            //  (N=2): clear entire line

            break;

        case 'S':
            // SU - scroll up
            //  N=line count

            break;

        case 'T':
            // SD - scroll down
            //  N=line count

            break;

        case 'h':
            if (np == 1) {
                switch (params[0]) {
                case 20:
                    // Set new line mode
                    // ?
                    break;
                }
            }
            break;

        case 'm':

            break;

        case 'i':
            //  (N=5: aux port on,
            //  (N=4: aux port off
            break;

        case 'n':
            //  (N=6: DSR device status report
            // Sends ESC[n;mR  where n=cursor row, m=cursor column
            break;

        case 's':
            // Save cursor position
            break;

        case 'u':
            // Restore cursor position
            break;

        default:
            break;
        }
    }

    void parse_char32(char32_t c)
    {
        switch (state) {
        case state_t::NORMAL:
            switch (c) {
            case ESC:
                state = state_t::IN_ESC;
                break;

            case CR:

            case LF:

            case VT:
            case FF:

            default:;
            }
            break;

        case state_t::IN_ESC:
            switch (c) {
            case '[':   // CSI - Control Sequence Introducer
                state = state_t::IN_CSI;
                csi.clear();
                break;

            case 'c':   // RIS - Reset to Initial State
                break;

            case '\\':  // ST - String Terminator
                break;
            case 'N':   // SS2 - Single Shift Two - Select G2 character set
                break;
            case 'O':   // SS3 - Single Shift Three - Select G3 character set
                break;
            case 'P':   // DCS  Device Control String
                break;
            case ']':   // OSC - Operating System Command
                break;
            case 'X':   // SOS - Start of string
                break;
            case '^':   // PM - Privacy Message
                break;
            case '_':   // APC - Application Program Command
                break;
            }
            break;

        case state_t::IN_CSI:
            // any number (including none) of "parameter bytes"
            //  in the range 0x30–0x3F (ASCII 0–9:;<=>?)
            // then, any number of intermediate bytes in the range 0x20–0x2F,
            // then, a single "final byte" in the range 0x40–0x7E
            if (c >= 0x30 && c < 0x40) {
                // parameter byte
                csi.append(1, c);
            } else if (c >= 0x20 && c < 0x30) {
                // intermediate byte
                csi.append(1, c);
            } else if (c >= 0x40 && c < 0x7F) {
                parse_csi(c);
                state = state_t::NORMAL;
            }
            break;
        }
    }

    void parse_byte(uint8_t c)
    {
        if (utf8_remain > 0) {
            if ((c & 0xC0) == 0x80) {
                utf8_codepoint <<= 6;
                utf8_codepoint |= c & 0x3F;
                if (--utf8_remain == 0) {
                    // Act on utf8_codepoint
                    return parse_char32(utf8_codepoint);
                }
            }
        } else {
            if (c < 0x80) {
                return parse_char32(c);
            } else if (c >= 0x80 && c <= 0x9F) {
                // C1 control codes

                return parse_char32(c);
            } else if (c >= 0xC0 && c < 0xE0) {
                // 110xxxxx
                // 2 byte (12-bit)

                utf8_remain = 1;
                utf8_codepoint = c & 0x1F;
            } else if (c >= 0xE0 && c < 0xF0) {
                // 1110xxxx
                // 3 byte (17-bit)

                utf8_remain = 2;
                utf8_codepoint = c & 0xF;
            } else if (c >= 0xF0 && c < 0xF8) {
                // 11110xxx
                // 4 byte UTF-8 (22 bit)

                utf8_remain = 3;
                utf8_codepoint = c & 0x7;
            } else if (unlikely(c >= 0xF8 && c < 0xFC)) {
                // 111110xx
                // 5 byte invalid UTF-8

                utf8_remain = 4;
                utf8_codepoint = c & 0x3;
            } else if (unlikely(c >= 0xFC && c < 0xFE)) {
                // 1111110x
                // 6 byte invalid UTF-8

                utf8_remain = 5;
                utf8_codepoint = c & 0x1;
            }
        }
    }

    void selectCharset(uintptr_t arg)
    {

    }
};

errno_t vt100_console_t::read_async(void *data, int64_t count, iocp_t *iocp)
{
    return errno_t::ENOSYS;
}

errno_t vt100_console_t::write_async(void *data, int64_t count, iocp_t *iocp)
{
    return errno_t::ENOSYS;
}

errno_t vt100_console_t::flush_async(iocp_t *iocp)
{
    return errno_t::ENOSYS;
}
