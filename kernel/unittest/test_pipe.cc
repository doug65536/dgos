#include "unittest.h"
#include "pipe.h"

__BEGIN_ANONYMOUS

UNITTEST(test_pipe_construct)
{
    pipe_t pipe;

    eq(size_t(0), pipe.capacity());
}

UNITTEST(test_pipe_reserve)
{
    pipe_t pipe;

    // 64KB, a normal capacity
    eq(true, pipe.reserve(16));
    eq(size_t(16 * PAGESIZE), pipe.capacity());
}

UNITTEST(test_pipe_easy_enqueue_dequeue)
{
    pipe_t pipe;

    // 64KB, a normal capacity
    eq(true, pipe.reserve(16));

    eq(size_t(16 * PAGESIZE), pipe.capacity());

    char data[64];

    size_t amount = (16 * PAGESIZE) - (16 * pipe.overhead());

    // Round down to multiple of 64
    amount &= -64;

    for (size_t sent = 0; sent < amount; sent += sizeof(data)) {
        for (size_t k = 0; k < sizeof(data) / sizeof(*data); ++k)
            data[k] = (char)sent + k;

        ssize_t block = pipe.enqueue(data, sizeof(data), 0);

        eq(ssize_t(sizeof(data)), block);
    }

    // Make sure it can't accept more, because it is completely full
    eq(0, pipe.enqueue(data, 1, 0));

    for (size_t sent = 0; sent < amount; sent += sizeof(data)) {
        ssize_t block = pipe.dequeue(data, sizeof(data), 0);

        eq(ssize_t(sizeof(data)), block);

        for (size_t k = 0; k < sizeof(data) / sizeof(*data); ++k)
            eq(char(sent + k), data[k]);
    }

    // Make sure it can't provide more, because it is completely empty
    eq(0, pipe.dequeue(data, sizeof(data), 0));
}

__END_ANONYMOUS
