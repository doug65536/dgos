#include "unittest.h"
#include "chrono.h"

UNITTEST(test_chrono_conversion)
{
    // Conversions that give a larger result
    std::chrono::days one_day(1);
    std::chrono::hours ihrs = one_day;
    std::chrono::minutes imin = one_day;
    std::chrono::seconds isec = one_day;
    std::chrono::milliseconds ims = one_day;
    std::chrono::microseconds ius = one_day;
    std::chrono::nanoseconds ins = one_day;

    eq(1, one_day.count());
    eq(24, ihrs.count());
    eq(1440, imin.count());
    eq(86400, isec.count());
    eq(86400000, ims.count());
    eq(86400000000, ius.count());
    eq(86400000000000, ins.count());

    // Conversions that give a reduced amount
    std::chrono::nanoseconds day_ns(86400000000000);
    std::chrono::microseconds dus = day_ns;
    std::chrono::milliseconds dms = day_ns;
    std::chrono::seconds dsec = day_ns;
    std::chrono::minutes dmin = day_ns;
    std::chrono::hours dhrs = day_ns;
    std::chrono::days dday = day_ns;

    eq(86400000000000, day_ns.count());
    eq(86400000000, dus.count());
    eq(86400000, dms.count());
    eq(86400, dsec.count());
    eq(1440, dmin.count());
    eq(24, dhrs.count());
    eq(1, dday.count());
}
