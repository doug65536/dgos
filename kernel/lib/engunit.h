#pragma once
#include "types.h"
#include "string.h"

template<typename T>
class engineering_t {
public:
    static constexpr char32_t const unit_lookup[] = {
        U'a',    // -60
        U'f',    // -50
        U'p',    // -40
        U'n',    // -30
        U'μ',    // -20
        U'm',    // -10
        0,      // <- [unit_base]
        U'k',    //  10
        U'M',    //  20
        U'G',    //  30
        U'T',    //  40
        U'P',    //  50
        U'E'     //  60
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

        if (unit_lookup[log1024_unit + 6]) {
            char encoded[5];
            size_t sz = ucs4_to_utf8(encoded, unit_lookup[log1024_unit + 6]);
            while (sz > 0)
                *--out = encoded[--sz];
        }

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
