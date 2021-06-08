#include <string.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/likely.h>

__BEGIN_ANONYMOUS

template<typename T>
static inline T lrot(T n, uint8_t s)
{
    return (n << s) | (n >> ((sizeof(n)*8) - s));
}

// Simplistic incremental hash function optimized for simplicity
class incremental_hash
{
public:
    incremental_hash() noexcept
    {
        reset();
    }

    explicit incremental_hash(char const *s) noexcept
    {
        reset();
        add_str(s);
    }

    incremental_hash(void const *s, size_t sz) noexcept
    {
        reset();
        add_mem((char const *)s, sz);
    }

    incremental_hash(incremental_hash const&) = default;
    incremental_hash(incremental_hash&&) = default;
    incremental_hash& operator=(incremental_hash const&) = default;
    incremental_hash& operator=(incremental_hash&&) = default;

    void reset() noexcept
    {
        h = size_t(0x4242424242424242);
        r = 3;
        c = 0;
    }

    size_t add_str(char const *s) noexcept
    {
        while (*s)
            add_byte(uint8_t(*s++));
        return h;
    }

    size_t add_mem(char const *s, size_t sz) noexcept
    {
        for (size_t i = 0; i < sz; ++i)
            add_byte(uint8_t(s[i]));
        return h;
    }

    size_t add_byte(uint8_t n) noexcept
    {
        h ^= lrot(n, r);
        h = lrot(h, 7);
        r += 3;
        r &= 7;
        ++c;
        return h;
    }

    size_t remove_byte(uint8_t n, size_t dist) noexcept
    {
        uint8_t byte_rot = ((dist * 3 + 3) + r) & 7;
        uint8_t mask_rot = (dist * 7) & 63;
        h ^= lrot(lrot(n, byte_rot), mask_rot);
        --c;
        return h;
    }

    size_t hash() const noexcept
    {
        return h;
    }

    size_t size() const noexcept
    {
        return c;
    }

private:
    size_t h;
    uint8_t r;
    size_t c;
};

__END_ANONYMOUS

char *strstr(char const *haystack, char const *needle)
{
    incremental_hash needle_hash(needle);
    incremental_hash search_hash;

    while (*haystack) {
        search_hash.add_byte(*haystack++);

        if (search_hash.size() == needle_hash.size()) {
            if (search_hash.hash() == needle_hash.hash() &&
                    !memcmp(haystack, needle, needle_hash.size()))
                return const_cast<char*>(haystack);

            search_hash.remove_byte(haystack[-needle_hash.size()],
                    needle_hash.size());
        }
    }
    return nullptr;
}

void *memmem(void const * restrict haystack, size_t haystacklen,
    void const * restrict needle, size_t needlelen)
{
    incremental_hash needle_hash(needle, needlelen);
    incremental_hash search_hash;

    char const *input = (char const *)haystack;

    for (size_t i = 0; i < haystacklen; ++i) {
        search_hash.add_byte(input[i]);

        if (search_hash.size() == needle_hash.size()) {
            if (unlikely(search_hash.hash() == needle_hash.hash()))
                if (likely(!memcmp(input, needle, needle_hash.size())))
                    return const_cast<char*>(input + i);

            search_hash.remove_byte(input[i - needle_hash.size()],
                    needle_hash.size());
        }
    }

    return nullptr;
}
