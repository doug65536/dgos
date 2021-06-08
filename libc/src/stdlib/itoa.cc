#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/likely.h>

// FIXME: duplicated
static char const formatter_hexlookup[16] = "0123456789abcdef0123456789ABCDEF";

int itoa(int n, char *txt, int base)
{
    char digits[sizeof(int) * CHAR_BIT + 2];
    char *end = digits + sizeof(digits);
    char *digit = end;
    *--digit = 0;

    bool pos = n >= 0 || (base == 16);
    unsigned v = (unsigned)n;

    n = pos ? n : -n;

    do {
        if (unlikely(digit == digits)) {
            errno = ERANGE;
            return -1;
        }

        int q = v / base;
        int r = v % base;
        *--digit = formatter_hexlookup[r];
        n = q;
    } while (v);

    if (!pos)
        *--digit = '-';

    size_t len = end - digit;
    memcpy(txt, digit, len + 1);

    return len;
}
