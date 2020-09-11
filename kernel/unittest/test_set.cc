#include "unittest.h"
#include "basic_set.h"
#include "stdlib.h"
#include "permute.h"

// Instantiate everything
template class ext::__basic_tree<int, void>;
template class ext::__basic_tree<int, void>::__basic_iterator<false, 1>;
template class ext::__basic_tree<int, void>::__basic_iterator<true, 1>;
template class ext::__basic_tree<int, void>::__basic_iterator<false, -1>;
template class ext::__basic_tree<int, void>::__basic_iterator<true, -1>;

#define HEAP_VALIDATE 0
#if HEAP_VALIDATE
#define heap_validate() eq(true, malloc_validate(false));
#else
#define heap_validate() ((void)0)
#endif

UNITTEST(test_set_int_default_construct)
{
    heap_validate();

    ext::set<int> c;
    eq(true, c.validate());

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

    heap_validate();
}

UNITTEST(test_set_int_insert_1)
{
    heap_validate();

    ext::set<int> c;

    ext::pair<ext::set<int>::iterator, bool> ins_pair = c.insert(1);

    ne(nullptr, &*ins_pair.first);
    eq(true, ins_pair.second);
    eq(1, *ins_pair.first);

    eq(true, c.validate());

    eq(size_t(1), c.size());
    eq(true, c.begin() + 1 == c.end());
    eq(true, c.cbegin() + 1 == c.cend());
    eq(true, c.rbegin() + 1 == c.rend());
    eq(true, c.crbegin() + 1 == c.crend());
    eq(true, c.end() - 1 == c.begin());
    eq(true, c.cend() - 1 == c.cbegin());
    eq(true, c.rend() - 1 == c.rbegin());
    eq(true, c.crend() - 1 == c.crbegin());
    eq(true, c.end() == c.find(42));
    eq(true, c.begin() == c.find(1));
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

    heap_validate();
}

UNITTEST(test_set_int_insert_dup)
{
    heap_validate();

    ext::set<int> c;

    ext::pair<ext::set<int>::iterator, bool> ins_pair = c.insert(1);

    ne(nullptr, &*ins_pair.first);
    eq(true, ins_pair.second);
    eq(1, *ins_pair.first);

    eq(true, c.validate());

    ins_pair = c.insert(1);

    ne(nullptr, &*ins_pair.first);
    eq(false, ins_pair.second);
    eq(1, *ins_pair.first);

    eq(true, c.validate());

    eq(size_t(1), c.size());
    eq(true, c.begin() + 1 == c.end());
    eq(true, c.cbegin() + 1 == c.cend());
    eq(true, c.rbegin() + 1 == c.rend());
    eq(true, c.crbegin() + 1 == c.crend());
    eq(true, c.end() - 1 == c.begin());
    eq(true, c.cend() - 1 == c.cbegin());
    eq(true, c.rend() - 1 == c.rbegin());
    eq(true, c.crend() - 1 == c.crbegin());
    eq(true, c.end() == c.find(42));
    eq(true, c.begin() == c.find(1));
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

    heap_validate();
}

UNITTEST(test_set_int_excess_iter_incdec_at_ends)
{
    heap_validate();

    ext::set<int> c;

    ext::set<int>::iterator mit = c.begin();
    ext::set<int>::const_iterator cit = c.begin();
    ext::set<int>::reverse_iterator rit = c.begin();
    ext::set<int>::const_reverse_iterator crit = c.begin();

    // Beginning is also end in empty set
    eq(true, c.end() == mit);
    eq(true, c.cend() == cit);
    eq(true, c.rend() == rit);
    eq(true, c.crend() == crit);

    // Increment inappropriately at end
    ++mit;
    ++cit;
    ++rit;
    ++crit;

    // And still at end, without blowing up
    eq(true, c.end() == mit);
    eq(true, c.cend() == cit);
    eq(true, c.rend() == rit);
    eq(true, c.crend() == crit);

    mit = c.begin();
    cit = c.begin();
    rit = c.begin();
    crit = c.begin();

    // Decrement inappropriately at begin
    --mit;
    --cit;
    --rit;
    --crit;

    // And still at begin, without blowing up
    eq(true, c.begin() == mit);
    eq(true, c.cbegin() == cit);
    eq(true, c.rbegin() == rit);
    eq(true, c.crbegin() == crit);

    heap_validate();
}

UNITTEST(test_set_int_construct_insert_churn)
{
    heap_validate();

    for (int i = 0; i < 100; ++i) {
        ext::set<int> c;
        for (int k = 0; k < 100; ++k) {
            ext::pair<ext::set<int>::iterator, bool> ins_pair = c.insert(k);

            ne(nullptr, &*ins_pair.first);
            eq(true, ins_pair.second);
            eq(k, *ins_pair.first);
        }
    }

    heap_validate();
}

UNITTEST(test_set_first_last)
{
    heap_validate();

    ext::set<int> c;
    for (int i = 0; i < 100; ++i)
        c.insert(i);

    for (int i = 0; i < 50; ++i) {
        //c.dump("firstlast");

        eq(i, *c.begin());
        eq(i, *c.cbegin());
        eq(99 - i, *c.rbegin());
        eq(99 - i, *c.crbegin());

        ext::set<int>::iterator fit = c.erase(c.begin());

        eq(true, fit == c.begin());

        ext::set<int>::iterator lit = c.erase(c.rbegin().current());

        eq(true, lit == c.end());

        if (i != 49) {
            eq(i + 1, *c.begin());
            eq(i + 1, *c.cbegin());
            eq(99 - i - 1, *c.rbegin());
            eq(99 - i - 1, *c.crbegin());
        } else {
            eq(true, c.empty());
            eq(true, c.begin() == c.end());
            eq(true, c.cbegin() == c.cend());
            eq(true, c.rbegin() == c.rend());
            eq(true, c.crbegin() == c.crend());
        }
    }
}

UNITTEST(test_set_int_insert_1k)
{
    heap_validate();

    ext::set<int> c;
    int e = 1000;

    for (int i = 0; i < e; ++i) {
        ext::pair<ext::set<int>::iterator, bool> ins_pair = c.insert(i);

        ne(nullptr, &*ins_pair.first);
        eq(true, ins_pair.second);
        eq(i, *ins_pair.first);

        eq(size_t(i) + 1, c.size());

    }

    eq(true, c.validate());

    eq(size_t(e), c.size());
    eq(true, c.begin() + e == c.end());
    eq(true, c.cbegin() + e == c.cend());
    eq(true, c.rbegin() + e == c.rend());
    eq(true, c.crbegin() + e == c.crend());
    eq(true, c.end() - e == c.begin());
    eq(true, c.cend() - e == c.cbegin());
    eq(true, c.rend() - e == c.rbegin());
    eq(true, c.crend() - e == c.crbegin());

    eq(true, c.end() == c.find(e));

    for (int i = 0; i < e; ++i) {
        eq(true, c.end() != c.find(i));
        eq(i, *c.find(i));
    }

    eq(true, c.begin() == c.find(0));
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

    heap_validate();
}

UNITTEST(test_set_lower_bound)
{
    ext::set<int> c;

    for (int i = 2; i < 10; i += 2) {
        ext::pair<ext::set<int>::iterator, bool> ins = c.insert(i);
        eq(i, *ins.first);
        eq(true, ins.second);
    }

    for (int i = 1; i < 10; ++i) {
        int expect = (i & 1) ? (i + 1) : i;
        ext::set<int>::iterator it = c.lower_bound(i);
        eq(i > 8, it == c.end());
        eq(true, i > 8 || *it == expect);
    }
}

UNITTEST(test_set_int_insert_reverse)
{
    heap_validate();

    ext::set<int> c;
    int e = 1000;

    for (int i = 0; i < e; ++i) {
        ext::pair<ext::set<int>::iterator, bool> ins_pair = c.insert(e - i);

        ne(nullptr, &*ins_pair.first);
        eq(true, ins_pair.second);
        eq(e - i, *ins_pair.first);

    }

    //c.dump();

    eq(true, c.validate());

    eq(size_t(e), c.size());
    eq(true, c.begin() + e == c.end());
    eq(true, c.cbegin() + e == c.cend());
    eq(true, c.rbegin() + e == c.rend());
    eq(true, c.crbegin() + e == c.crend());
    eq(true, c.end() - e == c.begin());
    eq(true, c.cend() - e == c.cbegin());
    eq(true, c.rend() - e == c.rbegin());
    eq(true, c.crend() - e == c.crbegin());

    eq(true, c.end() == c.find(e + 1));

    for (int i = 0; i < e; ++i) {
        eq(true, c.end() != c.find(e - i));
        eq(e - i, *c.find(e - i));
    }

    eq(true, c.begin() == c.find(1));
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

    heap_validate();
}

UNITTEST(test_set_int_simple_erase)
{
    heap_validate();

    ext::set<int> c;

    for (int i = 0; i < 8; ++i) {
        ext::pair<ext::set<int>::iterator, bool> ins_pair = c.insert(i);

        ne(nullptr, &*ins_pair.first);
        eq(true, ins_pair.second);
        eq(i, *ins_pair.first);

        eq(true, c.validate());
    }

    for (int n = 0; n < 8; ++n) {
        int i = 0;
        for (int const& v : c) {
            eq(n + i++, v);
            ge(8, i);
        }
        auto it = c.find(n);
        eq(true, c.end() != it);
        eq(true, c.validate());
        auto next = c.erase(it);
        eq(true, (n + 1 < 8 ? c.find(n + 1) : c.end()) == next);
    }

    eq(true, c.validate());

    int i = 0;
    for (int const& _ _unused: c)
        ++i;
    eq(0, i);
    eq(size_t(0), c.size());

    heap_validate();
}

UNITTEST(test_set_int_complex_erase)
{
    ext::set<int> c;

    int inserted_st = 0;
    int inserted_en = 0;

    for (int i = 0; i < 64 + 1024 + 64; ++i) {
        if (i < 64 + 1024) {
            ext::pair<ext::set<int>::iterator, bool>
                    ins_pair = c.insert(inserted_en++);

            ne(nullptr, &*ins_pair.first);
            eq(true, ins_pair.second);
            eq(inserted_en - 1, *ins_pair.first);
        }

        if (i >= 64) {
            auto it = c.find(inserted_st++);
            eq(true, c.end() != it);
            c.erase(it);
        }

        for (int i = inserted_st; i < inserted_en; ++i) {
            auto it = c.find(i);
            eq(true, c.end() != it);
            eq(i, *it);
        }


        eq(size_t(inserted_en) - inserted_st, c.size());
    }

    eq(true, c.validate());
}

UNITTEST(test_set_int_every_insert_permutation)
{
    ext::set<int> c;

    ext::vector<int> order;
    eq(true, order.reserve(8));

    order.clear();
    for (int i = 0; i < 8; ++i)
        order.push_back(i);

    do {
        c.clear();

        for (int i = 0; i < 8; ++i) {
            int key = order[i];

            ext::pair<ext::set<int>::iterator, bool>
                    ins_pair = c.insert(key);

            ne(nullptr, &*ins_pair.first);
            eq(true, ins_pair.second);
            eq(key, *ins_pair.first);

            eq(true, c.validate());

            for (int k = 0; k <= i; ++k) {
                key = order[i];
                auto it = c.find(key);
                eq(true, c.end() != it);
                eq(key, *it);
            }
        }
    } while (ext::next_permutation(order.begin(), order.end()));

    ext::reverse(order.begin(), order.end());
}

UNITTEST(test_set_int_every_erase_permutation)
{
    ext::vector<int> order;
    ext::set<int> c;

    for (int i = 0; i < 8; ++i)
        order.push_back(i);

    do {
        for (int i = 0; i < 8; ++i) {
            ext::pair<ext::set<int>::iterator, bool> ins_pair = c.insert(i);

            ne(nullptr, &*ins_pair.first);
            eq(true, ins_pair.second);
            eq(i, *ins_pair.first);
        }

        for (int i = 0; i < 8; ++i) {
            int key = order[i];

            eq(size_t(1), c.erase(key));

            eq(size_t(8 - i - 1), c.size());

            // Make sure all the ones we did not erase remain
            for (int k = i + 1; k < 8; ++k) {
                auto it = c.find(order[k]);
                eq(true, c.end() != it);
                eq(order[k], *it);
            }

            // Make sure all the ones erased are gone
            for (int k = 0; k <= i; ++k)
                eq(true, c.end() == c.find(order[k]));

            eq(true, c.validate());
        }
    } while (ext::next_permutation(order.begin(), order.end()));
}

UNITTEST(test_map_insert)
{
    ext::map<int, short> c;
    ext::pair<ext::map<int, short>::iterator, bool>
            ins = c.insert({42, 6500});
    eq(true, c.end() != ins.first);
    eq(42, ins.first->first);
    eq(6500, ins.first->second);
    eq(size_t(1), c.size());
    eq(6500, c[42]);
}

UNITTEST(test_map_index)
{
    ext::map<int, short> c;
    c[42] = 6500;
    c[43] = 8900;
    eq(size_t(2), c.size());
    eq(6500, c[42]);
    eq(8900, c[43]);
}

class destruct_watcher_t {
public:
    destruct_watcher_t()
        : value(0)
        , destruct_counter(nullptr)
    {
    }

    destruct_watcher_t(int value, int *destruct_counter)
        : value(value)
        , destruct_counter(destruct_counter)
    {
    }

    destruct_watcher_t(destruct_watcher_t&& rhs)
        : value(rhs.value)
        , destruct_counter(rhs.destruct_counter)
    {
        rhs.destruct_counter = nullptr;
    }

    destruct_watcher_t(destruct_watcher_t const& rhs)
        : value(rhs.value)
        , destruct_counter(rhs.destruct_counter)
    {
        if (rhs.destruct_counter)
            rhs.destruct_counter = nullptr;
    }

    destruct_watcher_t& operator=(destruct_watcher_t&& rhs)
    {
        value = rhs.value;
        destruct_counter = rhs.destruct_counter;
        rhs.destruct_counter = nullptr;
        return *this;
    }

    destruct_watcher_t& operator=(destruct_watcher_t const& rhs)
    {
        value = rhs.value;
        destruct_counter = rhs.destruct_counter;
        return *this;
    }

    ~destruct_watcher_t()
    {
        if (destruct_counter != nullptr)
            ++*destruct_counter;
    }

    bool operator<(destruct_watcher_t const& rhs) const
    {
        return value < rhs.value;
    }

    int value;
private:
    mutable int* destruct_counter;
};

UNITTEST(test_map_key_value_destruct)
{
    int key1_count = 0;
    int key2_count = 0;
    int val1_count = 0;
    int val2_count = 0;

    ext::map<destruct_watcher_t, destruct_watcher_t> c;
    c[destruct_watcher_t(42, &key1_count)] =
            destruct_watcher_t(6500, &val1_count);
    c[destruct_watcher_t(43, &key2_count)] =
            destruct_watcher_t(8900, &val2_count);
    eq(size_t(2), c.size());
    eq(6500, c[destruct_watcher_t(42, nullptr)].value);
    eq(8900, c[destruct_watcher_t(43, nullptr)].value);
    c.clear();
    eq(size_t(0), c.size());
    eq(key1_count, 1);
    eq(key2_count, 1);
    eq(val1_count, 1);
    eq(val2_count, 1);
}
