#include "math.h"
#include "types.h"
#include "likely.h"
#include "string.h"

#ifndef __x86_64__
#error Expected x86-64
#endif

int ilogbl(long double n)
{
    uint16_t e;
    memcpy(&e, (char*)&n + 8, sizeof(uint16_t));
    e &= 0x7FFF;

    if (e == 0)
        return INT_MIN;

    if (e < 0x7FFF)
        return (int)e - 0x3FFF;

    return INT_MAX;
}

#ifndef __DGOS_KERNEL__
long double nextafterl(long double n, long double t)
{
    uint64_t old_mantissa;
    uint16_t old_exponent;
    uint64_t new_mantissa;
    uint16_t new_exponent;
    uint8_t old_negative;
    uint8_t new_negative;

    memcpy(&old_mantissa, &n, sizeof(old_mantissa));
    memcpy(&old_exponent, (char*)&n + sizeof(uint64_t),
           sizeof(old_exponent));
    old_negative = old_exponent > 0x7FFF;
    old_exponent &= 0x7FFF;

    if ((n >= 0 && t > n) || (n < 0 && t < n)) {
        //
        // Away from zero

        new_negative = old_negative;

        if (old_exponent == 0 && old_mantissa == 0) {
            new_mantissa = 1;
            new_exponent = old_exponent;
            new_negative = t < 0;
        } else if (old_mantissa < 0xFFFFFFFFFFFFFFFFUL) {
            // Simply increment mantissa
            new_mantissa = old_mantissa + 1;
            new_exponent = old_exponent;
        } else {
            // Carry into exponent
            new_mantissa = (old_mantissa >> 1) + 1;
            if (old_exponent < 0x7FFFU) {
                // Increment exponent
                new_exponent = old_exponent + 1;
            } else {
                // Overflow into +infinity
                new_mantissa = 0;
                new_exponent = 0x7FFF;
            }
        }
    } else if ((n >= 0 && t < n) || (n < 0 && t < n)) {
        //
        // Toward zero

        new_negative = old_negative;

        if (old_exponent == 1 && old_mantissa == 0x8000000000000000UL) {
            // Transition to denormal
            new_exponent = 0;
            new_mantissa = 0x7FFFFFFFFFFFFFFFUL;
        } else if (old_exponent > 0 && old_mantissa == 0x8000000000000000UL) {
            // Borrow from exponent
            new_exponent = old_exponent - 1;
            new_mantissa = 0xFFFFFFFFFFFFFFFFUL;
        } else {
            // Simply decrement mantissa
            new_mantissa = old_mantissa - 1;
            new_exponent = old_exponent;
        }
    } else {
        return n;
    }

    new_exponent |= 0x8000 & -new_negative;

    memcpy(&n, &new_mantissa, sizeof(new_mantissa));
    memcpy((char*)&n, &new_exponent, sizeof(new_exponent));
    return n;
}
#endif
