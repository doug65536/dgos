#include "unittest.h"
#include "rbtree.h"

UNITTEST(test_set_int_default_construct)
{
    std::set<int> c;

    eq(size_t(0), c.size());
    eq(true, c.empty());
    eq(true, c.begin() == c.end());
    eq(true, c.cbegin() == c.cend());
    eq(true, c.rbegin() == c.rend());
    eq(true, c.crbegin() == c.crend());

    size_t count = 0;

    for (int item _unused : c)
        ++count;

    eq(size_t(0), count);
}

UNITTEST(test_set_int_insert_1)
{
    std::set<int> c;

    c.insert(1);

    eq(size_t(1), c.size());
    eq(true, c.begin() + 1 == c.end());
    eq(true, c.cbegin() + 1 == c.cend());
    eq(true, c.rbegin() + 1 == c.rend());
    eq(true, c.crbegin() + 1 == c.crend());
    eq(true, c.end() - 1 == c.begin());
    eq(true, c.cend() - 1 == c.cbegin());
    eq(true, c.rend() - 1 == c.rbegin());
    eq(true, c.crend() - 1 == c.crbegin());
    eq(c.end(), c.find(42));
    eq(c.begin(), c.find(1));
    eq(1, *c.begin());
    eq(1, *c.cbegin());
    eq(1, *c.rbegin());
    eq(1, *c.crbegin());

    int count = 0;

    for (int item : c) {
        eq(1, item);
        ++count;
    }

    eq(1, count);
}

UNITTEST(test_set_int_insert_dup)
{
    std::set<int> c;

    c.insert(1);
    c.insert(1);

    eq(size_t(1), c.size());
    eq(true, c.begin() + 1 == c.end());
    eq(true, c.cbegin() + 1 == c.cend());
    eq(true, c.rbegin() + 1 == c.rend());
    eq(true, c.crbegin() + 1 == c.crend());
    eq(true, c.end() - 1 == c.begin());
    eq(true, c.cend() - 1 == c.cbegin());
    eq(true, c.rend() - 1 == c.rbegin());
    eq(true, c.crend() - 1 == c.crbegin());
    eq(c.end(), c.find(42));
    eq(c.begin(), c.find(1));
    eq(1, *c.begin());
    eq(1, *c.cbegin());
    eq(1, *c.rbegin());
    eq(1, *c.crbegin());

    int count = 0;

    for (int item : c) {
        eq(1, item);
        ++count;
    }

    eq(1, count);
}

UNITTEST(test_set_int_insert_1k)
{
    std::set<int> c;
    int e = 1000;

    for (int i = 0; i < e; ++i)
        c.insert(i);

    eq(size_t(e), c.size());
    eq(true, c.begin() + e == c.end());
    eq(true, c.cbegin() + e == c.cend());
    eq(true, c.rbegin() + e == c.rend());
    eq(true, c.crbegin() + e == c.crend());
    eq(true, c.end() - e == c.begin());
    eq(true, c.cend() - e == c.cbegin());
    eq(true, c.rend() - e == c.rbegin());
    eq(true, c.crend() - e == c.crbegin());

    eq(c.end(), c.find(e));

    for (int i = 0; i < e; ++i) {
        ne(c.end(), c.find(i));
        eq(i, *c.find(i));
    }

    eq(c.begin(), c.find(0));
    eq(0, *c.begin());
    eq(0, *c.cbegin());
    eq(e - 1, *c.rbegin());
    eq(e - 1, *c.crbegin());

    int count = 0;

    for (int item : c) {
        eq(count, item);
        ++count;
    }

    eq(e, count);
}

UNITTEST(test_set_int_insert_reverse)
{
    std::set<int> c;
    int e = 1000;

    for (int i = 0; i < e; ++i)
        c.insert(e - i);

    eq(size_t(e), c.size());
    eq(true, c.begin() + e == c.end());
    eq(true, c.cbegin() + e == c.cend());
    eq(true, c.rbegin() + e == c.rend());
    eq(true, c.crbegin() + e == c.crend());
    eq(true, c.end() - e == c.begin());
    eq(true, c.cend() - e == c.cbegin());
    eq(true, c.rend() - e == c.rbegin());
    eq(true, c.crend() - e == c.crbegin());

    eq(c.end(), c.find(e + 1));

    for (int i = 0; i < e; ++i) {
        ne(c.end(), c.find(e - i));
        eq(e - i, *c.find(e - i));
    }

    eq(c.begin(), c.find(1));
    eq(1, *c.begin());
    eq(1, *c.cbegin());
    eq(e, *c.rbegin());
    eq(e, *c.crbegin());

    int count = 0;

    for (int item : c) {
        eq(count + 1, item);
        ++count;
    }

    eq(e, count);
}
