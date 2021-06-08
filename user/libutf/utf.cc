#include "utf.h"
#include "assert.h"

#if defined(__DGOS_KERNEL__) || defined(__DGOS_BOOTLOADER__)
#include "likely.h"
#else
#include <sys/likely.h>
#endif

#if !defined(__DGOS_KERNEL__) && !defined(__DGOS_BOOTLOADER__)
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#define EXPORT __attribute__((__visibility__("default")))
#else
#ifdef __DGOS_KERNEL__
#include "export.h"
#else
#define EXPORT __attribute__((__visibility__("default")))
#endif

#include "string.h"
#include "types.h"
#include "bswap.h"
#endif

// out should have room for at least 5 bytes
// if out is null, returns how many bytes it
// would have wrote to out, not including null terminator
// Returns 0 for values outside 0 <= in < 0x101000 range
// Always writes null terminator if out is not null
EXPORT size_t ucs4_to_utf8(char *out, char32_t in)
{
    size_t len;

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

EXPORT int ucs4_to_utf16(char16_t *out, char32_t in)
{
    if ((in > 0 && in < 0xD800) ||
            (in > 0xDFFF && in < 0x10000)) {
        if (out) {
            *out++ = (char16_t)in;
            *out = 0;
        }
        return 1;
    } else if (in > 0xFFFF && in < 0x110000) {
        in -= 0x10000;
        if (out) {
            *out++ = 0xD800 + ((in >> 10) & 0x3FF);
            *out++ = 0xDC00 + (in & 0x3FF);
            *out = 0;
        }
        return 2;
    }

    // Codepoint out of range or in surrogate range
    if (out)
        *out = 0;

    return 0;
}

// Returns 32 bit wide character
// Returns -1 on error
// If ret_end is not null, pointer to first
// byte after encoded character to *ret_end
// If the input is a null byte, and ret_end is not null,
// then *ret_end is set to point to the null byte, it
// is not advanced
EXPORT char32_t utf8_to_ucs4(char const *in, char const **ret_end)
{
    char32_t result = utf8_to_ucs4_upd(in);
    if (ret_end)
        *ret_end = in;
    return result;
}

EXPORT char32_t utf8_to_ucs4_upd(char const *&in)
{
    int n;

    if ((*in & 0x80) == 0) {
        n = *in++ & 0x7F;
    } else if ((*in & 0xE0) == 0xC0) {
        n = (*in++ & 0x1F) << 6;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= *in++ & 0x3F;
        else
            n |= -1;
    } else if ((*in & 0xF0) == 0xE0) {
        n = (*in++ & 0x0F) << 12;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= (*in++ & 0x3F) << 6;
        else
            n |= -1;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= *in++ & 0x3F;
        else
            n |= -1;
    } else if ((*in & 0xF8) == 0xF0) {
        n = (*in++ & 0x07) << 18;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= (*in++ & 0x3F) << 12;
        else
            n |= -1;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= (*in++ & 0x3F) << 6;
        else
            n |= -1;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= *in++ & 0x3F;
        else
            n |= -1;
    } else {
        ++in;
        n = -1;
    }

    return n;
}

// Same semantics as utf8_to_ucs4
EXPORT char32_t utf16_to_ucs4(char16_t const *in, char16_t const **ret_end)
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

EXPORT char32_t utf16_to_ucs4_upd(char16_t const *&in)
{
    return utf16_to_ucs4(in, &in);
}

// Same semantics as utf8_to_ucs4
EXPORT char32_t utf16be_to_ucs4(char16_t const *in, char16_t const **ret_end)
{
    char16_t in0 = 0;

    // Handle misalignment
    memcpy(&in0, in, sizeof(in0));

    in0 = ntohs(in0);

    if (in0 < 0xD800 || in0 > 0xDFFF) {
        if (ret_end)
            *ret_end = in + (in0 != 0);

        return in0;
    } else {
        char16_t in1 = ntohs(in[1]);

        if (in0 >= 0xD800 && in0 <= 0xDBFF &&
                in1 >= 0xDC00 && in1 <= 0xDFFF) {
            if (ret_end)
                *ret_end = in + 2;

            return ((in0 - 0xD800) << 10) |
                    ((in1 - 0xDC00) & 0x3FF);
        }
    }
    // Invalid surrogate pair
    return 0;
}

// Returns 0 on failure due to invalid UTF-8 or truncation
// due to insufficient buffer
// Returns output count (not including null terminator) on success
size_t utf8_to_utf16(char16_t *output, size_t out_size_words, char const *in)
{
    char16_t *out = output;
    char16_t *out_end = out + out_size_words;
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
            *out++ = char16_t(uint8_t(*in++));
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
            *out++ = char16_t(ch);
        } else if (out + 1 >= out_end) {
            // Surrogate pair would overrun output buffer
            return 0;
        } else {
            ch -= 0x10000;
            *out++ = char16_t(0xD800 + ((ch >> 10) & 0x3FF));
            *out++ = char16_t(0xDC00 + (ch & 0x3FF));
        }
    }

    if (out >= out_end) {
        // Output buffer would overrun
        return 0;
    }

    *out = 0;

    return out - output;
}

size_t utf8_count(char const *in)
{
    size_t count = 0;
    for (char const *p = in; *p; utf8_to_ucs4_upd(p))
        ++count;
    return count;
}

size_t utf16_count(char16_t const *in)
{
    size_t count;
    for (count = 0; *in; utf16_to_ucs4_upd(in))
        ++count;
    return count;
}

size_pair_t utf16_to_utf8(char *output, size_t output_sz,
                          char16_t const *input, size_t input_sz)
{
    char *output_end = output + output_sz;
    //char16_t const *input_end = input + input_sz;
    char16_t const *input_start = input;
    char *output_start = output;

    for (size_t i = 0; i < input_sz; ++i) {
        char32_t codepoint = utf16_to_ucs4_upd(input);
        size_t output_need = ucs4_to_utf8(nullptr, codepoint);
        if (unlikely(output + output_need > output_end))
            break;
        output += ucs4_to_utf8(output, codepoint);
    }

    assert(output >= output_start);
    assert(input >= input_start);

    return {
        size_t(output - output_start),
        size_t(input - input_start)
    };
}

size_pair_t tchar_to_utf8(char *output, size_t output_sz,
                     char const *input, size_t input_sz)
{
    size_t sz = input_sz < output_sz ? input_sz : output_sz;

    // Avoid splitting in the middle of a multibyte character
    while (input_sz > sz && ((input[sz] & 0xC0) == 0x80))
        --sz;

    strncpy(output, input, sz);

    return {
        sz,
        sz
    };
}

size_pair_t tchar_to_utf16(char16_t *output, size_t output_sz,
                           char const *input, size_t input_sz)
{
    char32_t codepoint;

    char const *in = input;
    char16_t *out = output;

    do {
        codepoint = utf8_to_ucs4_upd(in);
        out += ucs4_to_utf16(out, codepoint);
    } while (codepoint);

    return {
        size_t(out - output),
        size_t(in - input)
    };
}

size_pair_t tchar_to_utf16(char16_t * restrict output, size_t output_sz,
                           char16_t const * restrict input, size_t input_sz)
{
    size_t i;

    for (i = 0; i < output_sz && i < input_sz; ++i) {
        output[i] = input[i];

        if (!input[i])
            break;
    }

    i -= (i == output_sz);
    output[i] = 0;

    return {
        i,
        i
    };
}

size_pair_t tchar_to_utf8(char *output, size_t output_sz,
                          char16_t const *input, size_t input_sz)
{
    return utf16_to_utf8(output, output_sz, input, input_sz);
}

int tchar_strcmp_utf8(char16_t const *lhs, char const *rhs)
{
    int diff;
    char32_t lhs_codepoint;
    char32_t rhs_codepoint;
    do {
        lhs_codepoint = utf16_to_ucs4_upd(lhs);
        rhs_codepoint = utf8_to_ucs4_upd(rhs);

        diff = int(lhs_codepoint) - int(rhs_codepoint);
    } while (!diff && lhs_codepoint && rhs_codepoint);

    return diff;
}

int tchar_strcmp_utf8(char const *lhs, char const *rhs)
{
    return strcmp(lhs, rhs);
}
