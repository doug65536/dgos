#include "unittest.h"
#include "vector.h"
#include "printk.h"

static void still_0xEE_padded(unittest::unit *unit,
                              ext::vector<char> const& buffer,
                              size_t expect_len,
                              char const *file = __builtin_FILE(),
                              int line = __builtin_LINE())
{
    unit->lt(expect_len, buffer.size());
    unit->eq(buffer[expect_len], 0);

    char const *buf_end = buffer.data() + buffer.size();
    for (char const *it = buffer.data() + expect_len + 1; it < buf_end; ++it) {
        if (*it != char(0xEE))
            unit->fail(file, line);
    }
}

UNITTEST(test_printk_literal)
{
    ext::vector<char> buffer(size_t(64), char(0xEE));

    int len = snprintf(buffer.data(), buffer.size(), "Literal");

    eq(7, len);
    eq_str("Literal", buffer.data());
    still_0xEE_padded(this, buffer, len);
}

UNITTEST(test_printk_types)
{
    ext::vector<char> buffer(size_t(64), char(0xEE));

    int len = snprintf(buffer.data(), buffer.size(),
                       "%" PRId8, int8_t(-42));
    eq(3, len);
    eq_str("-42", buffer.data());

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);

    len = snprintf(buffer.data(), buffer.size(),
                       "%" PRId16, int16_t(-17495));
    eq(6, len);
    eq_str("-17495", buffer.data());

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);

    len = snprintf(buffer.data(), buffer.size(),
                       "%" PRId32, int32_t(-231098765));
    eq(10, len);
    eq_str("-231098765", buffer.data());

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);

    len = snprintf(buffer.data(), buffer.size(),
                       "%" PRId64, int64_t(-221118061136778721));
    eq(19, len);
    eq_str("-221118061136778721", buffer.data());

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);

    len = snprintf(buffer.data(), buffer.size(),
                       "%" PRIu8, uint8_t(135));
    eq(3, len);
    eq_str("135", buffer.data());

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);

    len = snprintf(buffer.data(), buffer.size(),
                       "%" PRIu16, uint16_t(17495));
    eq(5, len);
    eq_str("17495", buffer.data());

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);

    len = snprintf(buffer.data(), buffer.size(),
                       "%" PRIu32, uint32_t(2310987654));
    eq(10, len);
    eq_str("2310987654", buffer.data());

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);

    len = snprintf(buffer.data(), buffer.size(),
                       "%" PRIu64, uint64_t(3211180611367787214));
    eq(19, len);
    eq_str("3211180611367787214", buffer.data());
}

UNITTEST(test_printk_d)
{
    ext::vector<char> buffer(size_t(64), char(0xEE));

    int len = snprintf(buffer.data(), buffer.size(), "%d", 0);
    eq(1, len);
    eq_str("0", buffer.data());
    still_0xEE_padded(this, buffer, len);

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);
    len = snprintf(buffer.data(), buffer.size(), "%d", -42);
    eq(3, len);
    eq_str("-42", buffer.data());
    still_0xEE_padded(this, buffer, len);

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);
    len = snprintf(buffer.data(), buffer.size(), "%d", 42);
    eq(2, len);
    eq_str("42", buffer.data());
    still_0xEE_padded(this, buffer, len);

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);
    len = snprintf(buffer.data(), buffer.size(), "%d", 2147483647);
    eq(10, len);
    eq_str("2147483647", buffer.data());
    still_0xEE_padded(this, buffer, len);

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);
    len = snprintf(buffer.data(), buffer.size(), "%d", -2147483647-1);
    eq(11, len);
    eq_str("-2147483648", buffer.data());
    still_0xEE_padded(this, buffer, len);
}

UNITTEST(test_printk_u)
{
    ext::vector<char> buffer(size_t(64), char(0xEE));

    ext::fill_n(buffer.data(), buffer.size(), 0xEE);
    int len = snprintf(buffer.data(), buffer.size(), "%u", 4294967295U);
    eq(10, len);
    eq_str("4294967295", buffer.data());
    still_0xEE_padded(this, buffer, len);
}

#if 0
static struct {
    char const * const fmt; double n;
} float_formatter_cases[] = {
    { "%%17.5f     42.8      -> %17.5f\n", 42.8        },
    { "%%17.5f     42.8e+60  -> %17.5f\n", 42.8e+60    },
    { "%%17.5f     42.8e-60  -> %17.5f\n", 42.8e-60    },
    { "%%+17.5f    42.8e-60  -> %+17.5f\n", 42.8e-60   },
    { "%%17.5f    -42.8      -> %17.5f\n", -42.8       },
    { "%%17.5f    -42.8e+60  -> %17.5f\n", -42.8e+60   },
    { "%%17.5f    -42.8e-60  -> %17.5f\n", -42.8e-60   },
    { "%%017.5f    42.8      -> %017.5f\n", 42.8       },
    { "%%017.5f   -42.8e+60  -> %017.5f\n", -42.8e+60  },
    { "%%+017.5f   42.8      -> %+017.5f\n", 42.8      },
    { "%%+017.5f  -42.8e+60  -> %+017.5f\n", -42.8e+60 },

    { "%%17.5e     42.8      -> %17.5e\n", 42.8        },
    { "%%17.5e     42.8e+60  -> %17.5e\n", 42.8e+60    },
    { "%%17.5e     42.8e-60  -> %17.5e\n", 42.8e-60    },
    { "%%+17.5e    42.8e-60  -> %+17.5e\n", 42.8e-60   },
    { "%%17.5e    -42.8      -> %17.5e\n", -42.8       },
    { "%%17.5e    -42.8e+60  -> %17.5e\n", -42.8e+60   },
    { "%%17.5e    -42.8e-60  -> %17.5e\n", -42.8e-60   },
    { "%%017.5e    42.8      -> %017.5e\n", 42.8       },
    { "%%017.5e   -42.8e+60  -> %017.5e\n", -42.8e+60  },
    { "%%+017.5e   42.8      -> %+017.5e\n", 42.8      },
    { "%%+017.5e  -42.8e+60  -> %+017.5e\n", -42.8e+60 }
};

UNITTEST(test_printk_float_formatter)
{
    for (size_t i = 0, e = countof(float_formatter_cases); i < e; ++i)
        printdbg(float_formatter_cases[i].fmt, float_formatter_cases[i].n);
}

UNITTEST_SET_FLOAT(test_printk_float_formatter, true);
#endif

