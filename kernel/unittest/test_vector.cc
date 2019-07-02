#include "unittest.h"
#include "vector.h"

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
        eq(citer(it), cit);
        eq(criter(rit), crit);

        eq(i, *it);
        eq(i, *cit);
        eq(e - i - 1, *rit);
        eq(e - i - 1, *crit);

        ++it;
        ++cit;
        ++rit;
        ++crit;
    }

    eq(v.end(), it);
    eq(v.cend(), cit);
    eq(v.rend(), rit);
    eq(v.crend(), crit);

    for (int i = 0; i < e; ++i) {
        --it;
        --cit;
        --rit;
        --crit;

        eq(citer(it), cit);
        eq(criter(rit), crit);

        eq(e - i - 1, *it);
        eq(e - i - 1, *cit);
        eq(i, *rit);
        eq(i, *crit);
    }

    eq(v.begin(), it);
    eq(v.cbegin(), cit);
    eq(v.rbegin(), rit);
    eq(v.crbegin(), crit);
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

DISABLED_UNITTEST(test_vector_lifecycle)
{
    MockStats stats;
    std::vector<MockItem> v;
    bool ok = v.emplace_back(&stats);
    eq(true, ok);
    MockItem proto(&stats);
    ok = v.resize(42, proto);
    eq(true, ok);

    std::vector<MockItem> v2 = std::move(v);

    eq(42U, stats.nr_construct);
}
