#include "code16gcc.h"
#include "utf.h"

// Returns 0 on failure due to invalid UTF-8 or truncation due to insufficient buffer
// Returns output count (not including null terminator) on success
uint16_t utf8_to_utf16(uint16_t *output,
                       uint16_t out_size_words,
                       char const *in)
{
    uint16_t *out = output;
    uint16_t *out_end = out + out_size_words;
    uint16_t len;
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
            *out++ = (uint16_t)(uint8_t)*in++;
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
            *out++ = (uint16_t)ch;
        } else if (out + 1 >= out_end) {
            // Surrogate pair would overrun output buffer
            return 0;
        } else {
            ch -= 0x10000;
            *out++ = 0xD800 + ((ch >> 10) & 0x3FF);
            *out++ = 0xDC00 + (ch & 0x3FF);
        }
    }

    if (out >= out_end) {
        // Output buffer would overrun
        return 0;
    }

    *out++ = 0;

    return out - output - 1;
}
