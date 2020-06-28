#include "unittest.h"
#include "vector.h"

template class std::vector<int>;

struct MockStats {
    size_t nr_construct = 0;
    size_t nr_copycon = 0;
    size_t nr_movecon = 0;
    size_t nr_copyass = 0;
    size_t nr_moveass = 0;
    size_t nr_destruct = 0;
    size_t nr_ub = 0;
};

struct MockItem {
public:
    MockItem() = default;
    explicit MockItem(MockStats *stats)
        : stats(stats)
    {
        ++stats->nr_construct;
    }

    MockItem(MockItem const& rhs)
        : stats(rhs.stats)
    {
        stats->nr_ub += !!empty;
        ++stats->nr_copycon;
    }

    MockItem(MockItem&& rhs)
        : stats(rhs.stats)
    {

        rhs.empty = true;
        ++stats->nr_movecon;
    }

    MockItem& operator=(MockItem const &rhs)
    {
        stats = rhs.stats;
        ++stats->nr_copyass;
        return *this;
    }

    MockItem& operator=(MockItem&& rhs)
    {
        stats = rhs.stats;
        rhs.stats = nullptr;
        ++stats->nr_moveass;
        return *this;
    }

    ~MockItem()
    {
        ++stats->nr_destruct;
        stats = nullptr;
    }

    void set_stats(MockStats *stats)
    {
        this->stats = stats;
    }

private:
    MockStats *stats = nullptr;

    // true after being moved from
    bool empty = false;
};

UNITTEST(test_vector_construct_default)
{
    std::vector<int> v;
    eq(size_t(0), v.size());
    eq(true, v.empty());
}

UNITTEST(test_vector_construct_size_defval)
{
    std::vector<int> v(42);

    eq(42U, v.size());
    le(42U, v.capacity());
    eq(false, v.empty());

    eq(42, v.end() - v.begin());
    eq(42, v.cend() - v.cbegin());
    eq(42, v.rend() - v.rbegin());
    eq(42, v.crend() - v.crbegin());

    ne(nullptr, v.data());
    eq(0, *v.data());
    eq(0, v.front());
    eq(0, v.back());

    int count = 0;

    for (int item : v) {
        eq(0, item);
        ++count;
    }

    eq(42, count);
}

UNITTEST(test_vector_construct_size_23val)
{
    std::vector<int> v(42, 23);

    eq(size_t(42), v.size());
    le(size_t(42), v.capacity());
    eq(false, v.empty());

    eq(42, v.end() - v.begin());
    eq(42, v.cend() - v.cbegin());
    eq(42, v.rend() - v.rbegin());
    eq(42, v.crend() - v.crbegin());

    ne(nullptr, v.data());
    eq(23, *v.data());
    eq(23, v.front());
    eq(23, v.back());

    int count = 0;

    for (int item : v) {
        eq(23, item);
        ++count;
    }

    eq(42, count);
}

UNITTEST(test_vector_iterator)
{
    int e = 42;
    std::vector<int> v(e);

    for (int i = 0; i < e; ++i)
        v[i] = i;

    for (int i = 0; i < e; ++i)
        eq(i, v[i]);

    for (int i = 0; i < e; ++i)
        eq(i, v.begin()[i]);

    for (int i = 0; i < e; ++i)
        eq(i, v.cbegin()[i]);

    for (int i = 0; i < e; ++i)
        eq(e - i - 1, v.rbegin()[i]);

    for (int i = 0; i < e; ++i)
        eq(e - i - 1, v.crbegin()[i]);

    for (int i = 0; i < e; ++i)
        eq(e - i - 1, v.end()[-1 - i]);

    for (int i = 0; i < e; ++i)
        eq(e - i - 1, v.cend()[-1 - i]);

    for (int i = 0; i < e; ++i)
        eq(i, v.rend()[-1 - i]);

    for (int i = 0; i < e; ++i)
        eq(i, v.crend()[-1 - i]);

    using iter = std::vector<int>::iterator;
    using citer = std::vector<int>::const_iterator;
    using riter = std::vector<int>::reverse_iterator;
    using criter = std::vector<int>::const_reverse_iterator;

    iter it = v.begin();
    citer cit = v.cbegin();
    riter rit = v.rbegin();
    criter crit = v.crbegin();

    for (int i = 0; i < e; ++i) {
        eq_np(citer(it), cit);
        eq_np(criter(rit), crit);

        eq(i, *it);
        eq(i, *cit);
        eq(e - i - 1, *rit);
        eq(e - i - 1, *crit);

        ++it;
        ++cit;
        ++rit;
        ++crit;
    }

    eq_np(v.end(), it);
    eq_np(v.cend(), cit);
    eq_np(v.rend(), rit);
    eq_np(v.crend(), crit);

    for (int i = 0; i < e; ++i) {
        --it;
        --cit;
        --rit;
        --crit;

        eq_np(citer(it), cit);
        eq_np(criter(rit), crit);

        eq(e - i - 1, *it);
        eq(e - i - 1, *cit);
        eq(i, *rit);
        eq(i, *crit);
    }

    eq_np(v.begin(), it);
    eq_np(v.cbegin(), cit);
    eq_np(v.rbegin(), rit);
    eq_np(v.crbegin(), crit);
}

UNITTEST(test_vector_construct_size_fill)
{
    std::vector<int> v(42, 63);
    eq(42U, v.size());
    le(42U, v.capacity());
    eq(42, v.end() - v.begin());
    eq(42, v.cend() - v.cbegin());
    eq(42, v.rend() - v.rbegin());
    eq(42, v.crend() - v.crbegin());
    ne(nullptr, v.data());
    eq(63, v.front());
    eq(63, v.back());
    eq(false, v.empty());
}

UNITTEST(test_vector_construct_pointer_pair)
{
    int const input[] = {
        42,
        42042,
        42042042
    };

    std::vector<int> v(input, input + 3);
    eq(3U, v.size());
    le(3U, v.capacity());
    eq(3, v.end() - v.begin());
    eq(3, v.cend() - v.cbegin());
    eq(3, v.rend() - v.rbegin());
    eq(3, v.crend() - v.crbegin());
    ne(nullptr, v.data());
    eq(42, v.front());
    eq(42042042, v.back());
    eq(false, v.empty());
    eq(42042, v[1]);
}

// Lookup table to get bit-reversed 8 bit value from an 8 bit value
static uint8_t const bitrev[] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0,
    0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,

    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
    0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,

    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
    0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,

    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC,
    0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,

    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
    0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,

    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA,
    0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,

    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6,
    0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,

    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,

    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1,
    0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,

    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9,
    0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,

    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
    0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,

    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED,
    0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,

    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
    0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,

    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
    0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,

    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7,
    0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,

    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF,
    0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

UNITTEST(test_vector_push)
{
    std::vector<int> v;

    int count = 0;

    for (int i = 0; i < 1024; ++i) {
        v.push_back(i);

        eq(i, v[count]);
        eq(i, v.data()[count]);

        if (count >= 11)
            eq(11, v[11]);

        ++count;

        eq(size_t(count), v.size());
        le(size_t(count), v.capacity());
        eq(false, v.empty());
        ne(nullptr, v.data());
        eq(i, v.back());
        eq(0, v.front());
        eq(count, v.end() - v.begin());
        eq(count, v.cend() - v.cbegin());
        eq(count, v.rend() - v.rbegin());
        eq(count, v.crend() - v.crbegin());
    }

    le(size_t(1024), v.capacity());
    eq(1024, count);

    v.clear();

    eq(size_t(0), v.size());
}

UNITTEST(test_vector_pop)
{
    std::vector<int> v;

    for (int i = 0; i < 1024; ++i)
        v.push_back(i);

    for (int i = 1024; i; --i) {
        eq(i - 1, v.back());
        v.pop_back();
        eq(size_t(i - 1), v.size());
    }
    eq(size_t(0), v.size());
    eq(true, v.empty());
    eq(0, v.end() - v.begin());
    eq(0, v.cend() - v.cbegin());
    eq(0, v.rend() - v.rbegin());
    eq(0, v.crend() - v.crbegin());
}

UNITTEST(test_vector_insert_begin)
{
    std::vector<int> v;

    for (int i = 1024; i > 0; --i) {
        v.insert(v.begin(), i - 1);
        eq(i - 1, v.front());
    }

    for (int i = 0; i < 1024; ++i)
        eq(i, v[i]);
}

UNITTEST(test_vector_erase)
{
    std::vector<int> v;

    for (int i = 0; i < 1024; ++i)
        v.push_back(i);

    for (int i = 1024; i > 0; i -= 2)
    v.erase(v.begin() + (i - 2));
}

DISABLED_UNITTEST(test_vector_lifecycle)
{
    MockStats stats;
    std::vector<MockItem> v;
    bool ok = v.emplace_back(&stats);
    eq(true, ok);
    eq(size_t(1), stats.nr_construct);

    MockItem proto(&stats);
    eq(size_t(2), stats.nr_construct);

    ok = v.resize(42, proto);
    eq(41U, stats.nr_copycon);
    eq(1U, stats.nr_movecon);
    eq(true, ok);

    std::vector<MockItem> v2 = std::move(v);

    eq(1U, stats.nr_construct);
}

#include "rand.h"
#include "hash.h"


_const
static size_t mem_fill_value(size_t input)
{
    // Expect them at 8-byte boundaries, make all bits useful
    size_t n = ((input & UINT64_C(0xFFFFFFFFFFFF) >> 3)) *
            UINT64_C(6364136223846793005) +
            UINT64_C(1442695040888963407);
    n = (n << 32) | (n >> 32);
    return n;
}

_const
static uint8_t byte_from_sz(size_t sz)
{
    return ((sz & 0xFF) + ((sz >> 8) & 0xFF) + 1) & 0xFF;
}

UNITTEST(test_vector_random_malloc)
{
    std::vector<std::pair<std::unique_ptr<uint8_t[]>, size_t>> ptrs(1234);
    uint64_t seed = 42;

    size_t i = 0;
    for (size_t pass = 0; pass < 16; ++pass) {
        auto& stored_ptr = ptrs[i].first;
        size_t& stored_sz = ptrs[i].second;

        int sz = rand_r(&seed) % 65536;
        uint8_t *p = new (ext::nothrow) uint8_t[sz]();
        ne(nullptr, p);

        uint8_t expect = byte_from_sz(stored_sz);
        for (size_t k = 0; k < stored_sz; ++k)
            eq(expect, stored_ptr[k]);

        if (likely(p)) {
            memset(p, byte_from_sz(sz), sz);
            stored_sz = sz;
        } else {
            stored_sz = 0;
        }

        stored_ptr.reset(p);

        if (unlikely(++i >= ptrs.size()))
            i = 0;
    }
}
