#include <string.h>

int strncmp(char const *lhs, char const *rhs, size_t sz)
{
    int cmp = 0;
    if (sz) {
        do {
            cmp = (unsigned char)(*lhs) -
                    (unsigned char)(*rhs++);
        } while (--sz && cmp == 0 && *lhs++);
    }
    return cmp;
}
