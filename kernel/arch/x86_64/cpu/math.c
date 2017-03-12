#include "math.h"
#include "types.h"
#include "likely.h"
#include "string.h"

int ilogbl(long double n)
{
#if 1
    uint16_t e;
    memcpy(&e, (char*)&n + 8, sizeof(uint16_t));
    e &= 0x7FFF;
    return likely(e < 0x7FFF)
            ? (int)e - 0x3FFF
            : INT_MAX;
#else
    union {
        long double n;
        struct {
            uint64_t m:64;
            uint16_t e:15;
            uint8_t s:1;
        } __attribute__((packed)) p;
    } u;

    u.n = n;

    return likely(u.p.e < 0x7FFF)
            ? (int)u.p.e - 0x3FFF
            : INT_MAX;
#endif
}
