#include "unittest.h"
#include "vector.h"

UNITTEST(test_vector_construct_default)
{
    std::vector<int> v;
    eq(size_t(0), v.size());
    eq(true, v.empty());
}

UNITTEST(test_vector_construct_size)
{
    std::vector<int> v(42);
    eq(42U, v.size());
    le(42U, v.capacity());
    eq(42, v.end() - v.begin());
    eq(42, v.cend() - v.cbegin());
    eq(42, v.rend() - v.rbegin());
    eq(42, v.crend() - v.crbegin());
    ne(nullptr, v.data());
    eq(0, v.front());
    eq(0, v.back());
    eq(false, v.empty());
}

template<typename T>
static bool is_filled_with(std::vector<T> const& vec, T const& value)
{
    for (auto it = vec.begin(), e = vec.end(); it != e; ++it)
        if (*it != value)
            return false;
    for (auto it = vec.cbegin(), e = vec.cend(); it != e; ++it)
        if (*it != value)
            return false;
    for (auto it = vec.rbegin(), e = vec.rend(); it != e; ++it)
        if (*it != value)
            return false;
    for (auto it = vec.crbegin(), e = vec.crend(); it != e; ++it)
        if (*it != value)
            return false;
    return true;
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
    eq(true, is_filled_with(v, 63));
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

