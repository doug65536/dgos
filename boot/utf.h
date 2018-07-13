#pragma once

#include "types.h"

// Returns 0 on failure due to invalid UTF-8 or
// truncation due to insufficient buffer
// Returns output count (not including null terminator) on success
size_t utf8_to_utf16(uint16_t *output,
                       size_t out_size_words,
                       char const *in);

int utf16_to_ucs4(char16_t const *in, char16_t const **ret_end);

int ucs4_to_utf8(char *out, int in);
