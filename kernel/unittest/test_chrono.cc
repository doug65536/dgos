#include "unittest.h"
#include "chrono.h"
#include "type_traits.h"

__BEGIN_ANONYMOUS

// 2/3 / 1/6 = 4/1
static_assert(
        ext::is_same<
            ext::ratio_divide<
                ext::ratio<2, 3>::type,
                ext::ratio<1, 6>::type
            >::type,
            ext::ratio<4, 1>::type
        >::value,
        "ratio_divide does not work correctly");

// 2/3 * 1/6 = 1/9
static_assert(
        ext::is_same<
            ext::ratio_multiply<
                ext::ratio<2, 3>::type,
                ext::ratio<1, 6>::type
            >::type,
            ext::ratio<1, 9>::type
        >::value,
        "ratio_multiply does not work correctly");

// 2/3 + 1/6 = 5/6
static_assert(
        ext::is_same<
            ext::ratio_add<
                ext::ratio<2, 3>::type,
                ext::ratio<1, 6>::type
            >::type,
            ext::ratio<5, 6>::type
        >::value,
        "ratio_add does not work correctly");

// 2/3 - 1/6 = 1/2
static_assert(
        ext::is_same<
            ext::ratio_subtract<
                ext::ratio<2, 3>::type,
                ext::ratio<1, 6>::type
            >::type,
            ext::ratio<1, 2>::type
        >::value,
        "ratio_subtract does not work correctly");

UNITTEST(test_chrono_conversion)
{
    // Conversions that give a larger count
    ext::chrono::hours one_day(24);
    ext::chrono::minutes imin = one_day;
    ext::chrono::seconds isec = one_day;
    ext::chrono::milliseconds ims = one_day;
    ext::chrono::microseconds ius = one_day;
    ext::chrono::nanoseconds ins = one_day;

    eq(24U, one_day.count());
    eq(1440U, imin.count());
    eq(86400U, isec.count());
    eq(86400000U, ims.count());
    eq(86400000000U, ius.count());
    eq(86400000000000U, ins.count());

    // Conversions that give a reduced count
    ext::chrono::nanoseconds day_ns(86400000000000);
    ext::chrono::microseconds dus =
            ext::chrono::duration_cast<ext::chrono::microseconds>(day_ns);
    ext::chrono::milliseconds dms =
            ext::chrono::duration_cast<ext::chrono::milliseconds>(day_ns);
    ext::chrono::seconds dsec =
            ext::chrono::duration_cast<ext::chrono::seconds>(day_ns);
    ext::chrono::minutes dmin =
            ext::chrono::duration_cast<ext::chrono::minutes>(day_ns);
    ext::chrono::hours dhrs =
            ext::chrono::duration_cast<ext::chrono::hours>(day_ns);

    eq(86400000000000U, day_ns.count());
    eq(86400000000U, dus.count());
    eq(86400000U, dms.count());
    eq(86400U, dsec.count());
    eq(1440U, dmin.count());
    eq(24U, dhrs.count());
}

__END_ANONYMOUS
