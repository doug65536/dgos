#include "types.h"

// Returns 0 on failure due to invalid UTF-8 or truncation due to insufficient buffer
// Returns output count (not including null terminator) on success
uint16_t utf8_to_utf16(uint16_t *output,
                       uint16_t out_size_words,
                       char const *in);
