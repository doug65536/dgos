#include <string.h>
#include <stdint.h>

template<typename T>
static inline T lrot(T n, uint8_t s)
{
    return (n << s) |
            (n >> ((sizeof(n)*8) - s));
}

// Simplistic incremental hash function optimized for simplicity
class incremental_hash
{
public:
    incremental_hash()
    {
        reset();
    }

    incremental_hash(char const *s)
    {
        reset();
        add_str(s);
    }

    incremental_hash(incremental_hash const&) = default;
    incremental_hash(incremental_hash&&) = default;
    incremental_hash& operator=(incremental_hash const&) = default;
    incremental_hash& operator=(incremental_hash&&) = default;

    void reset()
    {
        h = size_t(0x4242424242424242);
        r = 3;
        c = 0;
    }

    size_t add_str(char const *s)
    {
        while (*s)
            add_byte(uint8_t(*s++));
        return h;
    }

    size_t add_byte(uint8_t n)
    {
        h ^= lrot(n, r);
        h = lrot(h, 7);
        r += 3;
        r &= 7;
        ++c;
        return h;
    }

    size_t remove_byte(uint8_t n, size_t dist)
    {
        uint8_t byte_rot = ((dist * 3 + 3) + r) & 7;
        uint8_t mask_rot = (dist * 7) & 63;
        h ^= lrot(lrot(n, byte_rot), mask_rot);
        --c;
        return h;
    }

    size_t hash() const
    {
        return h;
    }

    size_t size() const
    {
        return c;
    }

private:
    size_t h;
    uint8_t r;
    size_t c;
};

char *strstr(char const *haystack, char const *needle)
{
    incremental_hash needle_hash(needle);
    incremental_hash search_hash;

    while (*haystack)
    {
        search_hash.add_byte(*haystack++);

        if (search_hash.size() == needle_hash.size())
        {
            if (search_hash.hash() == needle_hash.hash() &&
                    !memcmp(haystack, needle, needle_hash.size()))
                return const_cast<char*>(haystack);

            search_hash.remove_byte(haystack[-needle_hash.size()],
                    needle_hash.size());
        }
    }
    return nullptr;
}
