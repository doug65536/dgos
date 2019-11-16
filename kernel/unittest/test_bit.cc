#include "unittest.h"
#include "bitsearch.h"
#include "algorithm.h"

UNITTEST(test_bit_uint64_t)
{
    for (uint64_t i = 0; i < 63; ++i) {
        uint64_t n = uint64_t(1) << i;
        uint64_t g = bit_log2(n);
        uint64_t h = bit_log2(n + std::max(uint64_t(1), n - 1));
        uint64_t k = bit_log2(n + 1);

        eq(i, g);
        eq(i + 1, h);
        eq(i + 1, k);
    }
}

UNITTEST(test_bit_uint32_t)
{
    for (uint32_t i = 0; i < 31; ++i) {
        uint32_t n = uint32_t(1) << i;
        uint32_t g = bit_log2(n);
        uint32_t h = bit_log2(n + std::max(uint32_t(1), n - 1));
        uint32_t k = bit_log2(n + 1);

        eq(i, g);
        eq(i + 1, h);
        eq(i + 1, k);
    }
}

UNITTEST(test_bit_uint16_t)
{
    for (uint16_t i = 0; i < 15; ++i) {
        uint16_t n = uint16_t(1) << i;
        uint16_t g = bit_log2(n);
        uint16_t h = bit_log2(n + std::max(uint16_t(1), uint16_t(n - 1)));
        uint16_t k = bit_log2(n + 1);

        eq(i, g);
        eq(i + 1, h);
        eq(i + 1, k);
    }
}

UNITTEST(test_bit_uint8_t)
{
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t n = uint8_t(1) << i;
        uint8_t g = bit_log2(n);
        uint8_t h = bit_log2(n + std::max(uint8_t(1), uint8_t(n - 1)));
        uint8_t k = bit_log2(n + 1);

        eq(i, g);
        eq(i + 1, h);
        eq(i + 1, k);
    }
}
