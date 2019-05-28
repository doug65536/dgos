#include "unittest.h"
#include "vector.h"

UNITTEST(test_vector_construct_default)
{
    std::vector<int> v;
    eq(size_t(0), v.size());
    eq(true, v.empty());
}

