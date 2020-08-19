#include "window.h"
#include <sys/likely.h>
#include <stdint.h>
#include <string.h>
#include <surface.h>
#include <stdlib.h>
#include <errno.h>

struct window_t;

template<typename T>
struct list {
    T **items;
    size_t size;
    size_t capacity;

    bool grow()
    {
        size_t new_capacity;

        if (capacity == 0)
            new_capacity = 15;
        else
            new_capacity = (capacity + 1) * 2 - 1;

        T **new_items = (T**)realloc(items, new_capacity);

        if (unlikely(!new_items)) {
            errno = ENOMEM;
            return false;
        }

        items = new_items;

        return true;
    }

    bool append(T *item)
    {
        if (size == capacity) {
            if (!grow())
                return false;
        }

        items[size++] = item;

        return true;
    }

    T *&operator[](size_t index)
    {
        return items[index];
    }

    T * const &operator[](size_t index) const
    {
        return items[index];
    }

    void erase_at(size_t index)
    {
        T *removed = items[index];

        memmove(items + index, items + (index + 1),
                (size - (index + 1)) * sizeof(*items));
    }
};

struct window_t {
    // Windows are kept in one big flat vector, but keep a parent link
    window_t *parent;

    // Position
    int32_t sx, sy;

    // Dimensions
    int32_t dx, dy;

    // Scroll position
    int32_t px, py;
};

struct reflow_fragment_t {

};
