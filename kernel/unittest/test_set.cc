#include "unittest.h"
#include "basic_set.h"
#include "stdlib.h"
#include "permute.h"

template class std::basic_set<int>;

UNITTEST(test_set_int_default_construct)
{
    eq(true, malloc_validate(false));

    std::set<int> c;
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

    eq(true, malloc_validate(false));
}

UNITTEST(test_set_int_insert_1)
{
    eq(true, malloc_validate(false));

    std::set<int> c;

    std::pair<std::set<int>::iterator, bool> ins_pair = c.insert(1);

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

    eq(true, malloc_validate(false));
}

UNITTEST(test_set_int_insert_dup)
{
    eq(true, malloc_validate(false));

    std::set<int> c;

    std::pair<std::set<int>::iterator, bool> ins_pair = c.insert(1);

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

    eq(true, malloc_validate(false));
}

UNITTEST(test_set_int_excess_iter_inc_at_end)
{
    eq(true, malloc_validate(false));

    std::set<int> c;

    std::set<int>::iterator mit = c.begin();
    std::set<int>::const_iterator cit = c.begin();
    std::set<int>::reverse_iterator rit = c.begin();
    std::set<int>::const_reverse_iterator crit = c.begin();

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

    eq(true, malloc_validate(false));
}

UNITTEST(test_set_int_construct_insert_churn)
{
    eq(true, malloc_validate(false));

    for (int i = 0; i < 100; ++i) {
        std::set<int> c;
        for (int k = 0; k < 100; ++k) {
            std::pair<std::set<int>::iterator, bool> ins_pair = c.insert(k);

            ne(nullptr, &*ins_pair.first);
            eq(true, ins_pair.second);
            eq(k, *ins_pair.first);

            eq(true, c.validate());
        }
    }

    eq(true, malloc_validate(false));
}

UNITTEST(test_set_int_insert_1k)
{
    eq(true, malloc_validate(false));

    std::set<int> c;
    int e = 1000;

    for (int i = 0; i < e; ++i) {
        std::pair<std::set<int>::iterator, bool> ins_pair = c.insert(i);

        ne(nullptr, &*ins_pair.first);
        eq(true, ins_pair.second);
        eq(i, *ins_pair.first);

        eq(size_t(i) + 1, c.size());

        eq(true, c.validate());
    }

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

    eq(true, malloc_validate(false));
}

UNITTEST(test_set_int_insert_reverse)
{
    eq(true, malloc_validate(false));

    std::set<int> c;
    int e = 1000;

    for (int i = 0; i < e; ++i) {
        std::pair<std::set<int>::iterator, bool> ins_pair = c.insert(e - i);

        ne(nullptr, &*ins_pair.first);
        eq(true, ins_pair.second);
        eq(e - i, *ins_pair.first);

        eq(true, c.validate());
    }

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

    eq(true, malloc_validate(false));
}

UNITTEST(test_set_int_simple_erase)
{
    eq(true, malloc_validate(false));

    std::set<int> c;

    for (int i = 0; i < 8; ++i) {
        std::pair<std::set<int>::iterator, bool> ins_pair = c.insert(i);

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
        eq(true, c.validate());
        eq(true, (n + 1 < 8 ? c.find(n + 1) : c.end()) == next);
    }

    int i = 0;
    for (int const& _ _unused: c)
        ++i;
    eq(0, i);
    eq(size_t(0), c.size());

    eq(true, malloc_validate(false));
}

UNITTEST(test_set_int_complex_erase)
{
    std::set<int> c;

    int inserted_st = 0;
    int inserted_en = 0;

    //asm("cli");
    for (int i = 0; i < 64 + 1024 + 64; ++i) {
        if (i < 64 + 1024) {
            std::pair<std::set<int>::iterator, bool> ins_pair =
                    c.insert(inserted_en++);

            ne(nullptr, &*ins_pair.first);
            eq(true, ins_pair.second);
            eq(inserted_en - 1, *ins_pair.first);

            eq(true, c.validate());
        }

        if (i >= 64) {
            auto it = c.find(inserted_st++);
            eq(true, c.end() != it);
            c.erase(it);
            eq(true, c.validate());
        }

        for (int i = inserted_st; i < inserted_en; ++i) {
            auto it = c.find(i);
            eq(true, c.end() != it);
            eq(i, *it);
        }

        eq(size_t(inserted_en) - inserted_st, c.size());
    }
    //asm("sti");
}

UNITTEST(test_set_int_every_insert_permutation)
{
    std::set<int> c;

    std::vector<int> order;
    eq(true, order.reserve(8));

    order.clear();
    for (int i = 0; i < 8; ++i)
        order.push_back(i);

    for (int seed = 0; seed < 8; ++seed) {
        c.clear();

        do {
            c.clear();
            for (int i = 0; i < 8; ++i) {
                int key = order[i];

                std::pair<std::set<int>::iterator, bool>
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
        } while (std::next_permutation(order.begin(), order.end()));

        std::reverse(order.begin(), order.end());
    }
}

UNITTEST(test_set_int_every_erase_order)
{
    std::set<int> c;

    for (int seed = 0; seed < 8; ++seed) {
        c.clear();

        for (int i = 0; i < 8; ++i) {
            std::pair<std::set<int>::iterator, bool> ins_pair = c.insert(i);

            ne(nullptr, &*ins_pair.first);
            eq(true, ins_pair.second);
            eq(i, *ins_pair.first);

            eq(true, c.validate());
        }

        //c.dump("before erase");

        for (int i = 0; i < 8; ++i) {
            int key = seed ^ i;

            //dbgout << "Erasing " << key << '\n';

            eq(size_t(1), c.erase(key));

            //c.dump("after erase");

            eq(true, c.validate());

            // Make sure all the ones we did not erase remain
            for (int k = i + 1; k < 8; ++k) {
                auto it = c.find(seed ^ k);
                eq(true, c.end() != it);
                eq(seed ^ k, *it);
            }

            // Make sure all the ones erased are gone
            for (int k = 0; k <= i; ++k)
                eq(true, c.end() == c.find(seed ^ k));
        }
    }
}
