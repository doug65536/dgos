#include "unittest.h"
#include "vector.h"
#include "printk.h"

static void still_0xEE_padded(unittest::unit *unit,
                              std::vector<char> const& buffer)
{
    char const *str_end = strchr(buffer.data(), 0);
    char const *buf_end = buffer.data() + buffer.size();
    for (char const *it = str_end + 1; it < buf_end; ++it) {
        if (*it != char(0xEE)) {
            unit->fail();
        }
    }
}

UNITTEST(test_printk_literal)
{
    std::vector<char> buffer(size_t(64), char(0xEE));

    snprintf(buffer.data(), buffer.size(), "Literal");

    eq("Literal", buffer.data());
    still_0xEE_padded(this, buffer);
}

UNITTEST(test_printk_d)
{
    std::vector<char> buffer(size_t(64), char(0xEE));

    snprintf(buffer.data(), buffer.size(), "%d", 0);
    eq("0", buffer.data());
    still_0xEE_padded(this, buffer);

    std::fill_n(buffer.data(), buffer.size(), 0xEE);
    snprintf(buffer.data(), buffer.size(), "%d", -42);
    eq("-42", buffer.data());
    still_0xEE_padded(this, buffer);

    std::fill_n(buffer.data(), buffer.size(), 0xEE);
    snprintf(buffer.data(), buffer.size(), "%d", 42);
    eq("42", buffer.data());
    still_0xEE_padded(this, buffer);

    std::fill_n(buffer.data(), buffer.size(), 0xEE);
    snprintf(buffer.data(), buffer.size(), "%d", 2147483647);
    eq("2147483647", buffer.data());
    still_0xEE_padded(this, buffer);

    std::fill_n(buffer.data(), buffer.size(), 0xEE);
    snprintf(buffer.data(), buffer.size(), "%d", -2147483647-1);
    eq("-2147483648", buffer.data());
    still_0xEE_padded(this, buffer);
}

UNITTEST(test_printk_u)
{
    std::vector<char> buffer(size_t(64), char(0xEE));

    std::fill_n(buffer.data(), buffer.size(), 0xEE);
    snprintf(buffer.data(), buffer.size(), "%u", 4294967295U);
    eq("4294967295", buffer.data());
    still_0xEE_padded(this, buffer);
}
