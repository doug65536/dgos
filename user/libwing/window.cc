#include "window.h"
#include <sys/likely.h>
#include <stdint.h>
#include <surface.h>
#include <stdlib.h>
#include <errno.h>

struct window_t;

struct window_list {
    window_t **windows;
    size_t size;
    size_t capacity;

    bool append(window_t *window)
    {
        if (size == capacity) {
            size_t new_capacity = capacity
                    ? (((capacity * 2) + 511) & -512) - 8
                    : 504;

            window_t **new_windows = (window_t**)realloc(windows, new_capacity);

            if (unlikely(!new_windows)) {
                errno = ENOMEM;
                return false;
            }

            windows = new_windows;
        }

        windows[size++] = window;
        return true;
    }

    window_t *&operator[](size_t index)
    {
        return windows[index];
    }

    window_t * const &operator[](size_t index) const
    {
        return windows[index];
    }
};

struct window_t {
    int32_t sx;
    int32_t sy;
    int32_t dx;
    int32_t dy;
    window_list children;

};
