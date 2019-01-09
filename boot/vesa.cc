#include "vesa.h"
#include "vesainfo.h"
#include "farptr.h"
#include "string.h"
#include "malloc.h"
#include "screen.h"
#include "paging.h"
#include "cpu.h"

#define VESA_DEBUG 1
#if VESA_DEBUG
#define VESA_TRACE(...) PRINT("vesa: " __VA_ARGS__)
#else
#define VESA_TRACE(...) ((void)0)
#endif


_const
static unsigned gcd(unsigned a, unsigned b)
{
    if (!a || !b)
        return a > b ? a : b;

    while (a != b) {
        if (a > b)
            a -= b;
        else
            b -= a;
    }

    return a;
}

void aspect_ratio(uint16_t *n, uint16_t *d, uint16_t w, uint16_t h)
{
    uint16_t div = gcd(w, h);
    *n = w / div;
    *d = h / div;
}
