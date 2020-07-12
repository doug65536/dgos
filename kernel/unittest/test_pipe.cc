#include "unittest.h"
#include "pipe.h"

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

    for (size_t sent = 0; sent < (64 << 10); sent += sizeof(data)) {
        for (size_t k = 0; k < sizeof(data) / sizeof(*data); ++k)
            data[k] = (char)sent + k;

        ssize_t block = pipe.enqueue(data, sizeof(data), INT64_MAX);

        eq(ssize_t(sizeof(data)), block);

        printdbg("sent %zd\n", sent + block);
    }

    // Make sure it can't accept more, because it is completely full
    eq(0, pipe.enqueue(data, 1, 0));


    for (size_t sent = 0; sent < (64 << 10); sent += sizeof(data)) {
        ssize_t block = pipe.dequeue(data, sizeof(data), INT64_MAX);

        eq(ssize_t(sizeof(data)), block);

        for (size_t k = 0; k < sizeof(data) / sizeof(*data); ++k)
            eq(char(sent + k), data[k]);
    }
}
