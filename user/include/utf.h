#pragma once
#include <stdint.h>
#include <stddef.h>

#ifndef restrict
#define restrict __restrict
#endif

#ifdef __cplusplus
extern "C" {
#endif

size_t ucs4_to_utf8(char *out, char32_t in);
int ucs4_to_utf16(char16_t *out, char32_t in);

char32_t utf8_to_ucs4(char const *in, char const **ret_end);
char32_t utf8_to_ucs4_upd(char const *&in);
char32_t utf16_to_ucs4(char16_t const *in, char16_t const **ret_end);
char32_t utf16_to_ucs4_upd(char16_t const *&in);


char32_t utf16be_to_ucs4(char16_t const *in, char16_t const **ret_end);

size_t utf8_count(char const *in);
size_t utf16_count(char16_t const *in);

size_t utf8_to_utf16(char16_t *output, size_t out_size_words, char const *in);

struct size_pair_t {
    size_t output_produced;
    size_t input_consumed;
};

size_pair_t utf16_to_utf8(char *output, size_t output_sz,
                          char16_t const *input, size_t input_sz);

#ifdef __cplusplus
}
#endif


static inline size_t ucs4_to_tchar(char *out, char32_t in) 
{
    return ucs4_to_utf8(out, in);
}

static inline size_t ucs4_to_tchar(char16_t *out, char32_t in)
{
    return ucs4_to_utf16(out, in);
}

static inline char32_t tchar_to_ucs4(char const *in, char const **ret_end)
{
    return utf8_to_ucs4(in, ret_end);
}

static inline char32_t tchar_to_ucs4_upd(char const *&in)
{
    return utf8_to_ucs4_upd(in);
}

static inline char32_t tchar_to_ucs4(char16_t const *in, char16_t const **ret_end)
{
    return utf16_to_ucs4(in, ret_end);
}

static inline char32_t tchar_to_ucs4_upd(char16_t const *&in)
{
    return utf16_to_ucs4_upd(in);
}

static inline size_t tchar_count(char const *in)
{
    return utf8_count(in);
}

static inline size_t tchar_count(char16_t const *in)
{
    return utf16_count(in);
}

int tchar_strcmp_utf8(char const * restrict lhs, char const *rhs);
int tchar_strcmp_utf8(char16_t const * restrict lhs, char const *rhs);


size_pair_t tchar_to_utf8(char * restrict output, size_t output_sz,
                          char const * restrict input, size_t input_sz);

size_pair_t tchar_to_utf8(char * restrict output, size_t output_sz,
                          char16_t const * restrict input, size_t input_sz);


size_pair_t tchar_to_utf16(char16_t * restrict output, size_t output_sz,
                           char const * restrict input, size_t input_sz);

size_pair_t tchar_to_utf16(char16_t * restrict output, size_t output_sz,
                           char16_t const * restrict input, size_t input_sz);

#ifdef __efi
static inline size_pair_t tchar_to_utf16(
        char16_t * restrict output, size_t output_sz,
        wchar_t const * restrict input, size_t input_sz)
{
    return tchar_to_utf16(output, output_sz, (char16_t*)input, input_sz);
}
#endif
