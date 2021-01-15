#include "debug.h"
#include "string.h"
#include "utf.h"

#ifdef __x86_64__
#include "x86/cpu_x86.h"
#endif

static bool use_serial = true;

static void e9out(void const *data, size_t len)
{
#ifdef __x86_64__
    if (use_serial) {
        outsb(0x3f8, data, len);
    } else {
        outsb(0xe9, data, len);
    }
#endif
}

void debug_out(char const *s, ptrdiff_t len)
{
//    if (len < 0)
//        len = strlen(s);
//    // No flow control!
//    e9out(s, len);
}

void debug_out(char16_t const *s, ptrdiff_t len)
{
//    if (len < 0)
//        len = strlen(s);

//    char16_t const *end = s + len;

//    // Allocate worst case UTF-8 output buffer
//    char16_t const *in = s;
//    char *buf = (char*)__builtin_alloca(len*4+1);
//    char *out = buf;
//    ptrdiff_t enc_len = 0;
//    while (in < end) {
//        char32_t codepoint = utf16_to_ucs4_upd(in);
//        enc_len += ucs4_to_utf8(out + enc_len, codepoint);
//    }
//    e9out(buf, enc_len);
}
