#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

void swab(const void *src, void *dst, ssize_t sz)
{
    uint16_t const *s = (uint16_t const *)src;
    uint16_t *d = (uint16_t*)dst;
    for (ssize_t i = 0; i < sz; i += 2)
        d[i] = __builtin_bswap16(s[i]);
}
