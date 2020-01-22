#pragma once
#include "types.h"

template<typename T>
class engineering_t {
public:
    static constexpr char const unit_lookup[] = {
        'a',    // -60
        'f',    // -50
        'p',    // -40
        'n',    // -30
        'u',    // -20
        'm',    // -10
        0,      // <- [unit_base]
        'k',    //  10
        'M',    //  20
        'G',    //  30
        'T',    //  40
        'P',    //  50
        'E'     //  60
    };

//    static constexpr size_t const unit_base = 6;

    engineering_t(T value, int log1024_unit = 0)
    {
        set_value(value, log1024_unit);
    }

    void set_value(T value, int log1024_unit = 0)
    {
        int shr = 0;
        out = text + sizeof(text);
        *--out = 0;

        if (value >= (UINT64_C(1) << 60)) {
            log1024_unit += 6;
            value >>= 10;
            shr = 50;
        } else if (value >= (UINT64_C(1) << 50)) {
            log1024_unit += 5;
            shr = 50;
        } else if (value >= (UINT64_C(1) << 40)) {
            log1024_unit += 4;
            shr = 40;
        } else if (value >= (UINT64_C(1) << 30)) {
            log1024_unit += 3;
            shr = 30;
        } else if (value >= (UINT64_C(1) << 20)) {
            log1024_unit += 2;
            shr = 20;
        } else if (value >= (UINT64_C(1) << 10)) {
            log1024_unit += 1;
            shr = 10;
        }

        if (log1024_unit < -6 || log1024_unit > 6) {
            *--out = '*';
            *--out = '*';
            *--out = '*';
            return;
        }

        if (unit_lookup[log1024_unit + 6])
            *--out = unit_lookup[log1024_unit + 6];

        // Convert into tenths decimal representation
        value *= 10;
        value >>= shr;

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

    char const *ptr() const
    {
        return out;
    }

    operator char const *() const
    {
        return ptr();
    }

private:
    char text[32];
    char *out;
};

extern template class engineering_t<uintptr_t>;
