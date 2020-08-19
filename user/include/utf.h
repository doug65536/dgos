#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ucs4_to_utf8(char *out, char32_t in);
int ucs4_to_utf16(char16_t *out, char32_t in);
char32_t utf8_to_ucs4(char const *in, char const **ret_end);
char32_t utf8_to_ucs4_upd(char const *&in);
char32_t utf16_to_ucs4(char16_t const *in, char16_t const **ret_end);
char32_t utf16_to_ucs4_upd(char16_t const *&in);
char32_t utf16be_to_ucs4(char16_t const *in, char16_t const **ret_end);
size_t utf8_count(char const *in);
size_t utf16_count(char16_t const *in);
size_t utf8_to_utf16(char16_t *output, size_t out_size_words, char const *in);

#ifdef __cplusplus
}
#endif
