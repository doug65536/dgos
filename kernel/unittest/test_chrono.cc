#include "unittest.h"
#include "chrono.h"
#include "type_traits.h"

// 2/3 / 1/6 = 4/1
static_assert(
        std::is_same<
            std::ratio_divide<
                std::ratio<2, 3>::type,
                std::ratio<1, 6>::type
            >::type,
            std::ratio<4, 1>::type
        >::value,
        "ratio_divide does not work correctly");

// 2/3 * 1/6 = 1/9
static_assert(
        std::is_same<
            std::ratio_multiply<
                std::ratio<2, 3>::type,
                std::ratio<1, 6>::type
            >::type,
            std::ratio<1, 9>::type
        >::value,
        "ratio_multiply does not work correctly");

// 2/3 + 1/6 = 5/6
static_assert(
        std::is_same<
            std::ratio_add<
                std::ratio<2, 3>::type,
                std::ratio<1, 6>::type
            >::type,
            std::ratio<5, 6>::type
        >::value,
        "ratio_add does not work correctly");

// 2/3 - 1/6 = 1/2
static_assert(
        std::is_same<
            std::ratio_subtract<
                std::ratio<2, 3>::type,
                std::ratio<1, 6>::type
            >::type,
            std::ratio<1, 2>::type
        >::value,
        "ratio_subtract does not work correctly");

UNITTEST(test_chrono_conversion)
{
    // Conversions that give a larger count
    std::chrono::hours one_day(24);
    std::chrono::minutes imin = one_day;
    std::chrono::seconds isec = one_day;
    std::chrono::milliseconds ims = one_day;
    std::chrono::microseconds ius = one_day;
    std::chrono::nanoseconds ins = one_day;

    eq(24U, one_day.count());
    eq(1440U, imin.count());
    eq(86400U, isec.count());
    eq(86400000U, ims.count());
    eq(86400000000U, ius.count());
    eq(86400000000000U, ins.count());

    // Conversions that give a reduced count
    std::chrono::nanoseconds day_ns(86400000000000);
    std::chrono::microseconds dus =
            std::chrono::duration_cast<std::chrono::microseconds>(day_ns);
    std::chrono::milliseconds dms =
            std::chrono::duration_cast<std::chrono::milliseconds>(day_ns);
    std::chrono::seconds dsec =
            std::chrono::duration_cast<std::chrono::seconds>(day_ns);
    std::chrono::minutes dmin =
            std::chrono::duration_cast<std::chrono::minutes>(day_ns);
    std::chrono::hours dhrs =
            std::chrono::duration_cast<std::chrono::hours>(day_ns);

    eq(86400000000000U, day_ns.count());
    eq(86400000000U, dus.count());
    eq(86400000U, dms.count());
    eq(86400U, dsec.count());
    eq(1440U, dmin.count());
    eq(24U, dhrs.count());
}
