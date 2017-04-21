#include "priorityqueue.h"

#include "assert.h"

C_ASSERT_ISPO2(sizeof(priqueue_t<int>));
C_ASSERT_ISPO2(sizeof(priqueue_t<int>));

#define TEST 0

#if TEST

void priqueue_test_t::test()
{
    priqueue_t<int> queue(
                &priqueue_test_t::priqueue_cmp,
                &priqueue_test_t::priqueue_swapped, this);

    queue.push(42);
    queue.push(11);
    queue.push(38);
    queue.push(1);
    queue.push(99);

    int x;
    x = queue.pop();
    x = queue.pop();
    x = queue.pop();
    x = queue.pop();
    x = queue.pop();
}

int priqueue_test_t::priqueue_cmp(int const &a, int const &b, void *)
{
    return a < b ? -1 : a > b ? 1 : 0;
}

void priqueue_test_t::priqueue_swapped(int const&, int const&, void *)
{
}

priqueue_test_t priqueue_test;

#endif
