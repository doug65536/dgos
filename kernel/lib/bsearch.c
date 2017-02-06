#include "bsearch.h"

// Returns the index of the matching item,
// Otherwise returns the ones complement of the insertion point
// The array is not really accessed, it does have to point to memory
intptr_t binary_search(void *va, size_t count, size_t item_size,
                     void *k, int (*cmp)(void *v, void *k, void *c),
                     void *c)
{
    size_t st = 0;
    size_t en = count;
    size_t mid;
    int diff = -1;

    while (st < en) {
        // Compute middle point
        mid = st + ((en - st) >> 1);

        // Comparator returns negative value if item < key
        diff = cmp((char*)va + (mid * item_size), k, c);

        if (diff < 0)
            en = mid;
        else
            st = mid;
    }

    if (st < count)
        diff = cmp((char*)va + (st * item_size), k, c);

    return st ^ -(diff != 0);
}
