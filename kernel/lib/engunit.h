#pragma once
#include "types.h"
#include "string.h"
#include "printk.h"

template<typename T>
class engineering_t {
public:
    static char32_t const unit_lookup[];

    static constexpr size_t const unit_base = 6;

    constexpr engineering_t(T value, int log_unit = 0, bool binary = false)
    {
        if (!binary)
            set_value(value, log_unit, 1000);
        else
            set_value(value, log_unit, 1024);
    }

    constexpr ~engineering_t()
    {
        //printdbg("engineering_t destructed\n");
    }

    constexpr void set_value(T value, int logN_unit = 0, int base = 1000)
    {
//        printdbg("value=%zx, logN_unit=%d, base=%d\n",
//                 uintptr_t(value), logN_unit, base);

        uint64_t div = 0;
        int shr = 0;
        out = text + sizeof(text);
        *--out = 0;

        if (base == 1000) {
            if (uint64_t(value) >= UINT64_C(1000000000000000000)) {//1e18
                logN_unit += 6;
                value /= 1000;
                div = UINT64_C(1000000000000000);
            } else if (uint64_t(value) >= UINT64_C(1000000000000000)) {//1e15
                logN_unit += 5;
                div = UINT64_C(1000000000000000);
            } else if (uint64_t(value) >= UINT64_C(1000000000000)) {//1e12
                logN_unit += 4;
                div = UINT64_C(1000000000000);
            } else if (uint64_t(value) >= UINT64_C(1000000000)) {//1e9
                logN_unit += 3;
                div = UINT64_C(1000000000);
            } else if (uint64_t(value) >= UINT64_C(1000000)) {//1e6
                logN_unit += 2;
                div = UINT64_C(1000000);
            } else if (uint64_t(value) >= UINT64_C(1000)) {//1e3
                logN_unit += 1;
                div = UINT64_C(1000);
            } else {//1e0
                div = 1;
            }
        } else if (base == 1024) {
            if (uint64_t(value) >= (UINT64_C(1) << 60)) {//Ei
                logN_unit += 6;
                value >>= 10;
                shr = 50;
            } else if (uint64_t(value) >= (UINT64_C(1) << 50)) {//Pi
                logN_unit += 5;
                shr = 50;
            } else if (uint64_t(value) >= (UINT64_C(1) << 40)) {//Ti
                logN_unit += 4;
                shr = 40;
            } else if (uint64_t(value) >= (UINT64_C(1) << 30)) {//Gi
                logN_unit += 3;
                shr = 30;
            } else if (uint64_t(value) >= (UINT64_C(1) << 20)) {//Mi
                logN_unit += 2;
                shr = 20;
            } else if (uint64_t(value) >= (UINT64_C(1) << 10)) {//Ki
                logN_unit += 1;
                shr = 10;
            } else {
                shr = 0;
            }

            if (logN_unit < -6 || logN_unit > 6) {
                *--out = '*';
                *--out = '*';
                *--out = '*';
                return;
            }
        } else {
            *--out = '?';
            return;
        }

        if (logN_unit < -6 || logN_unit > 6) {
            *--out = '*';
            *--out = '*';
            *--out = '*';
            return;
        }

        if (engineering_t<uint64_t>::unit_lookup[logN_unit + 6]) {
            char encoded[5];
            size_t sz = ucs4_to_utf8(encoded, engineering_t<uint64_t>::
                                     unit_lookup[logN_unit + 6]);
            while (sz > 0)
                *--out = encoded[--sz];
        }

        // Convert into tenths decimal representation
        value *= 10;

        if (base == 1024)
            value >>= shr;
        else
            value /= div;

        int whol = value / 10;
        int frac = value % 10;

        if (whol >= 10 || frac == 0) {
            // Can't fit decimal or it would be pointless .0
            do {
                *--out = '0' + (whol % 10);
                whol /= 10;
            } while (whol);
        } else {
            *--out = '0' + frac;
            *--out = '.';
            *--out = '0' + whol;
        }
    }

    constexpr char const *ptr() const
    {
        return out;
    }

    constexpr operator char const *() const
    {
        return ptr();
    }

private:
    char text[32];
    char *out;
};

template<typename T>
char32_t const engineering_t<T>::unit_lookup[] = {
    U'a',    // -60
    U'f',    // -50
    U'p',    // -40
    U'n',    // -30
    U'µ',    // -20
    U'm',    // -10
    0,      // <- [unit_base]
    U'k',    //  10
    U'M',    //  20
    U'G',    //  30
    U'T',    //  40
    U'P',    //  50
    U'E'     //  60
};

extern template class engineering_t<uint64_t>;
