#include "unittest.h"
#include "mm.h"
#include "user_mem.h"

UNITTEST(test_mm_copy_user_from_null)
{
    uint8_t *m = (uint8_t*)mm_alloc_space(16384);
    mmap(m + 4096, 4096, PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

    for (size_t st = 0; st < 4096; ++st) {
        for (size_t en = st; en < 4096; ++en) {
            memset(m + 4096, 0xCC, 4096);

            size_t sz = en - st;
            uint8_t *b = m + 4096 + st;
            bool ok = mm_copy_user(b, nullptr, sz);
            eq(true, ok);

            for (size_t i = 0; i < 4096; ++i) {
                eq(i >= st && i < en ? 0 : 0xCC,  m[i + 4096]);
            }
        }
    }
}