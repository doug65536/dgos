#include "utf.h"

// Returns 0 on failure due to invalid UTF-8 or truncation
// due to insufficient buffer
// Returns output count (not including null terminator) on success
size_t utf8_to_utf16(uint16_t *output, size_t out_size_words, char const *in)
{
    uint16_t *out = output;
    uint16_t *out_end = out + out_size_words;
    size_t len;
    uint32_t ch;

    while (*in) {
        if ((*in & 0xF8) == 0xF0) {
            ch = *in++ & 0x03;
            len = 4;
        } else if ((*in & 0xF0) == 0xE0) {
            ch = *in++ & 0x07;
            len = 3;
        } else if ((*in & 0xE0) == 0xC0) {
            ch = *in++ & 0x0F;
            len = 2;
        } else if ((*in & 0x80) != 0) {
            // Invalid, too long or character begins with 10xxxxxx
            return 0;
        } else if (out < out_end) {
            *out++ = uint16_t(uint8_t(*in++));
            continue;
        } else {
            // Output buffer overrun
            return 0;
        }

         while (--len) {
            if ((*in & 0xC0) == 0x80) {
                ch <<= 6;
                ch |= *in++ & 0x3F;
            } else {
                // Invalid, byte isn't 10xxxxxx
                return 0;
            }
        }

        if (ch >= 0xD800 && ch < 0xE000) {
            // Invalid UTF-8 in surrogate range
            return 0;
        }

        if (ch == 0) {
            // Overlong null character not allowed
            return 0;
        }
        if (out >= out_end) {
            // Output buffer would overrun
            return 0;
        }
        if (ch < 0x10000) {
            // Single UTF-16 character
            *out++ = uint16_t(ch);
        } else if (out + 1 >= out_end) {
            // Surrogate pair would overrun output buffer
            return 0;
        } else {
            ch -= 0x10000;
            *out++ = uint16_t(0xD800 + ((ch >> 10) & 0x3FF));
            *out++ = uint16_t(0xDC00 + (ch & 0x3FF));
        }
    }

    if (out >= out_end) {
        // Output buffer would overrun
        return 0;
    }

    *out = 0;

    return out - output;
}

int utf16_to_ucs4(char16_t const *in, char16_t const **ret_end)
{
    if (in[0] < 0xD800 || in[0] > 0xDFFF) {
        if (ret_end)
            *ret_end = in + (*in != 0);
        return *in;
    } else if (in[0] >= 0xD800 && in[0] <= 0xDBFF &&
            in[1] >= 0xDC00 && in[1] <= 0xDFFF) {
        if (ret_end)
            *ret_end = in + 2;
        return ((in[0] - 0xD800) << 10) |
                ((in[1] - 0xDC00) & 0x3FF);
    }

    // Invalid surrogate pair
    return 0;
}

// out should have room for at least 5 bytes
// If out is null, nothing is written to it
// Returns how many bytes it would have wrote to out,
// not including null terminator
// Returns 0 for values outside 0 <= in < 0x101000 range
// Always writes null terminator if out is not null
int ucs4_to_utf8(char *out, char32_t in)
{
    int len;
    if (in < 0x80) {
        if (out) {
            *out++ = (char)in;
            *out++ = 0;
        }
        return 1;
    }

    if (in < 0x800) {
        len = 2;
    } else if (in < 0x10000) {
        len = 3;
    } else if (in < 0x110000) {
        len = 4;
    } else {
        // Invalid
        if (out)
            *out++ = 0;
        return 0;
    }

    if (out) {
        int shift = len - 1;

        *out++ = (char)((signed char)0x80 >> shift) |
                (in >> (6 * shift));

        while (--shift >= 0)
            *out++ = 0x80 | ((in >> (6 * shift)) & 0x3F);

        *out++ = 0;
    }

    return len;
}
