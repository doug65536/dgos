#pragma once
#include <stdint.h>
#include <stddef.h>

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

size_t utf16_to_utf8(char *output, size_t output_sz,
                     char16_t const *input, size_t input_sz);

#ifdef __cplusplus
}
#endif

int tchar_strcmp_utf8(char const *lhs, char const *rhs);
int tchar_strcmp_utf8(char16_t const *lhs, char const *rhs);

size_t tchar_to_utf8(char *output, size_t output_sz,
                     char const *input, size_t input_sz);

size_t tchar_to_utf8(char *output, size_t output_sz,
                     char16_t const *input, size_t input_sz);
