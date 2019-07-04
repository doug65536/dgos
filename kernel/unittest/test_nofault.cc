#include "unittest.h"
#include "mm.h"
#include "user_mem.h"

UNITTEST(test_mm_copy_user_from_null)
{
    uint8_t *m = (uint8_t*)mm_alloc_space(PAGE_SIZE * 3);
    mmap(m + PAGE_SIZE, PAGE_SIZE,
         PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

    for (size_t st = 0; st < PAGE_SIZE; ++st) {
        if (st > 256 && st < PAGE_SIZE - 256)
            st = PAGE_SIZE - 256;

        for (size_t en = st; en < PAGE_SIZE; ++en) {
            if (en > 256 && en < PAGE_SIZE - 256)
                en = PAGE_SIZE - 256;

            if (en - st > 512)
                continue;

            memset(m + PAGE_SIZE, 0xCC, PAGE_SIZE);

            size_t sz = en - st;
            uint8_t *b = m + PAGE_SIZE + st;
            bool ok = mm_copy_user(b, nullptr, sz);
            eq(true, ok);

            for (size_t i = 0; i < PAGE_SIZE; ++i) {
                auto expect = i >= st && i < en ? 0 : 0xCC;
                eq(expect,  m[i + PAGE_SIZE]);
            }
        }
    }

    munmap(m, PAGE_SIZE * 3);
}

UNITTEST(test_mm_copy_user)
{
    uint8_t *m = (uint8_t*)mm_alloc_space(PAGE_SIZE * 3);
    mmap(m + PAGE_SIZE, PAGE_SIZE,
         PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

    uint8_t *s = (uint8_t*)mmap(nullptr, PAGE_SIZE,
                                PROT_READ | PROT_WRITE,
                                MAP_POPULATE, -1, 0);

    for (size_t st = 0; st < 4096; ++st)
        s[st] = st + (st >> 8);

    for (size_t st = 0; st < PAGE_SIZE; ++st) {
        if (st > 256 && st < PAGE_SIZE - 256)
            st = PAGE_SIZE - 256;

        for (size_t en = st; en < PAGE_SIZE; ++en) {
            if (en > 256 && en < PAGE_SIZE - 256)
                en = PAGE_SIZE - 256;

            if (en - st > 512)
                continue;

            memset(m + PAGE_SIZE, 0xCC, PAGE_SIZE);

            size_t sz = en - st;
            uint8_t *b = m + PAGE_SIZE + st;
            bool ok = mm_copy_user(b, s, sz);
            eq(true, ok);

            for (size_t i = 0; i < PAGE_SIZE; ++i) {
                auto expect = i >= st && i < en
                        ? s[i - st] : 0xCC;
                eq(expect, m[i + PAGE_SIZE]);
            }
        }
    }

    munmap(m, PAGE_SIZE * 3);
    munmap(s, PAGE_SIZE);
}
