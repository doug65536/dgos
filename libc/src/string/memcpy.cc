#include <string.h>

void *memcpy(void *restrict lhs, void const *restrict rhs, size_t sz)
{
    auto d = (char *)lhs;
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__ (
        "rep movsb"
        : "+D" (lhs), "+S" (rhs), "+c" (sz)
        :
        : "memory"
    );
#else
    auto s = (char const *)rhs;
    for (size_t i = 0; i < sz; ++i)
        d[i] = s[i];
#endif
    return d;
}
