#include "unittest.h"
#include "vector.h"
#include "printk.h"

static void still_0xEE_padded(unittest::unit *unit,
                              std::vector<char> const& buffer,
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
    std::vector<char> buffer(size_t(64), char(0xEE));

    int len = snprintf(buffer.data(), buffer.size(), "Literal");

    eq(7, len);
    eq("Literal", buffer.data());
    still_0xEE_padded(this, buffer, len);
}

UNITTEST(test_printk_d)
{
    std::vector<char> buffer(size_t(64), char(0xEE));

    int len = snprintf(buffer.data(), buffer.size(), "%d", 0);
    eq(1, len);
    eq("0", buffer.data());
    still_0xEE_padded(this, buffer, len);

    std::fill_n(buffer.data(), buffer.size(), 0xEE);
    len = snprintf(buffer.data(), buffer.size(), "%d", -42);
    eq(3, len);
    eq("-42", buffer.data());
    still_0xEE_padded(this, buffer, len);

    std::fill_n(buffer.data(), buffer.size(), 0xEE);
    len = snprintf(buffer.data(), buffer.size(), "%d", 42);
    eq(2, len);
    eq("42", buffer.data());
    still_0xEE_padded(this, buffer, len);

    std::fill_n(buffer.data(), buffer.size(), 0xEE);
    len = snprintf(buffer.data(), buffer.size(), "%d", 2147483647);
    eq(10, len);
    eq("2147483647", buffer.data());
    still_0xEE_padded(this, buffer, len);

    std::fill_n(buffer.data(), buffer.size(), 0xEE);
    len = snprintf(buffer.data(), buffer.size(), "%d", -2147483647-1);
    eq(11, len);
    eq("-2147483648", buffer.data());
    still_0xEE_padded(this, buffer, len);
}

UNITTEST(test_printk_u)
{
    std::vector<char> buffer(size_t(64), char(0xEE));

    std::fill_n(buffer.data(), buffer.size(), 0xEE);
    int len = snprintf(buffer.data(), buffer.size(), "%u", 4294967295U);
    eq(10, len);
    eq("4294967295", buffer.data());
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

