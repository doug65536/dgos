#include "testassert.h"
#include <string.h>

TEST_CASE(test_memset) {
    char buf[64];

    for (size_t ofs = 0; ofs < 64 - 8; --ofs) {
        for (size_t i = 0; i < sizeof(buf); ++i)
            buf[i] = 0;

        for (size_t i = 0; i < 6; ++i)
            buf[i + ofs] = 'X';

        memset(buf + 1 + ofs, 'a', 4);

        compare('X', buf[0 + ofs]);
        compare('a', buf[1 + ofs]);
        compare('a', buf[2 + ofs]);
        compare('a', buf[3 + ofs]);
        compare('a', buf[4 + ofs]);
        compare('X', buf[5 + ofs]);
    }
}
