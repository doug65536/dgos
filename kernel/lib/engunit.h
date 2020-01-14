#pragma once
#include "types.h"

template<typename T>
class engineering_t {
public:
    engineering_t(T value)
    {
        set_value(value);
    }

    void set_value(T value)
    {
        int shr = 0;
        out = text + sizeof(text);
        *--out = 0;

        if (value >= (UINT64_C(1) << 50)) {
            *--out = 'P';
            value >>= 10;
            shr = 40;
        } else if (value >= (UINT64_C(1) << 40)) {
            *--out = 'T';
            shr = 40;
        } else if (value >= (UINT64_C(1) << 30)) {
            *--out = 'G';
            shr = 30;
        } else if (value >= (UINT64_C(1) << 20)) {
            *--out = 'M';
            shr = 20;
        } else if (value >= (UINT64_C(1) << 10)) {
            *--out = 'K';
            shr = 10;
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
            *--out = '0' + (frac / 100);
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
