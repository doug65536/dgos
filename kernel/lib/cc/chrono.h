#pragma once
#include "types.h"
#include "ratio.h"
#include "type_traits.h"
#include "time.h"
#include "numeric.h"
#include "numeric_limits.h"
#include "export.h"

__BEGIN_NAMESPACE_STD
__BEGIN_NAMESPACE(chrono)

template<typename _Rep, typename _Period = ratio<1> >
class duration;

__END_NAMESPACE //chrono

template<typename _Rep, typename _Period>
class common_type<
        std::chrono::duration<_Rep, _Period>,
        std::chrono::duration<_Rep, _Period>>
{
public:
    using type = std::chrono::duration<_Rep, _Period>;
};

template<typename _Rep1, typename _Period1, typename _Rep2, typename _Period2>
class common_type<
        std::chrono::duration<_Rep1, _Period1>,
        std::chrono::duration<_Rep2, _Period2>>
{
public:
    using type = std::chrono::duration<
        typename common_type<_Rep1, _Rep2>::type,
        typename std::ratio<
            gcd(_Period1::num, _Period2::num),
            lcm(_Period1::den, _Period2::den)
        >::type
     >;
};

__BEGIN_NAMESPACE(chrono)

template<typename _ToDuration, typename _Rep, typename _Period>
constexpr _ToDuration duration_cast(duration<_Rep, _Period> const& rhs);

template<typename _Rep, typename _Period>
class duration
{
public:
    using rep = _Rep;
    using period = _Period;

    duration(rep __tick_count);

    duration(duration const& rhs);

    template<typename _Rep2, typename _Period2>
    duration(duration<_Rep2, _Period2> const& rhs)
        : __tick_count(duration_cast<duration, _Rep2, _Period2>(
                           rhs).__tick_count)
    {
    }

    rep count() const;

    duration operator-() const;

    duration operator+() const;

    duration operator+(duration const& rhs);

    duration operator-(duration const& rhs);

    duration& operator++();

    duration operator++(int);

    duration& operator--();

    duration operator--(int);

private:

    rep __tick_count;
};

template<typename _Rep, typename _Period>
duration<_Rep,_Period>
duration<_Rep,_Period>::operator--(int)
{
    return duration(__tick_count--);
}

template<typename _Rep, typename _Period>
duration<_Rep,_Period> &
duration<_Rep,_Period>::operator--()
{
    --__tick_count;
    return *this;
}

template<typename _Rep, typename _Period>
duration<_Rep,_Period>
duration<_Rep,_Period>::operator++(int)
{
    return duration(__tick_count++);
}

template<typename _Rep, typename _Period>
duration<_Rep,_Period> &
duration<_Rep,_Period>::operator++()
{
    ++__tick_count;
    return *this;
}

template<typename _Rep, typename _Period>
duration<_Rep,_Period>
duration<_Rep,_Period>::operator-(duration const& rhs)
{
    return duration(__tick_count - rhs.__tick_count);
}

template<typename _Rep, typename _Period>
duration<_Rep,_Period>
duration<_Rep,_Period>::operator+(duration const& rhs)
{
    return duration(__tick_count + rhs.__tick_count);
}

template<typename _Rep, typename _Period>
duration<_Rep,_Period>
duration<_Rep,_Period>::operator+() const
{
    return *this;
}

template<typename _Rep, typename _Period>
duration<_Rep,_Period>
duration<_Rep,_Period>::operator-() const
{
    return duration(-__tick_count);
}

template<typename _Rep, typename _Period>
typename duration<_Rep,_Period>::rep
duration<_Rep,_Period>::count() const
{
    return __tick_count;
}

template<typename _Rep, typename _Period>
duration<_Rep,_Period>::duration(duration const& rhs)
    : __tick_count(rhs.__tick_count)
{
}

template<typename _Rep, typename _Period>
duration<_Rep,_Period>::duration(rep __tick_count)
    : __tick_count(__tick_count)
{
}

template<typename _Rep1, typename _Period1,
         typename _Rep2, typename _Period2>
typename common_type<duration<_Rep1,_Period1>,
    duration<_Rep2,_Period2>>::type
constexpr operator+(duration<_Rep1,_Period1> const& lhs,
                    duration<_Rep2,_Period2> const& rhs)
{
    using T = typename common_type<
        duration<_Rep1,_Period1>,
        duration<_Rep2,_Period2>>::type;

    return T(T(lhs).count() + T(rhs).count());
}

template<typename _Rep1, typename _Period1,
         typename _Rep2, typename _Period2>
typename std::common_type<duration<_Rep1,_Period1>,
    duration<_Rep2,_Period2>>::type
constexpr operator-(duration<_Rep1,_Period1> const& lhs,
                    duration<_Rep2,_Period2> const& rhs)
{
    using T = typename common_type<
        duration<_Rep1,_Period1>,
        duration<_Rep2,_Period2>>::type;

    return T(T(lhs).count() - T(rhs).count());
}

template<typename _Rep1, typename _Period1,
         typename _Rep2>
duration<typename std::common_type<_Rep1,_Rep2>::type, _Period1>
constexpr operator*(duration<_Rep1,_Period1> const& lhs, _Rep2 const& rhs)
{
    using T = typename common_type<
        duration<_Rep1,_Period1>,
        duration<_Rep2,_Period1>>::type;

    return T(T(lhs).count() * T(duration<_Rep2, _Period1>(rhs)).count());
}

template<typename _Rep1, typename _Rep2,
         typename _Period1>
duration<typename std::common_type<_Rep1,_Rep2>::type, _Period1>
constexpr operator*(_Rep1 const& lhs, duration<_Rep2,_Period1> const& rhs)
{
    using T = typename common_type<
        duration<_Rep1,_Period1>,
        duration<_Rep2,_Period1>>::type;

    return T(T(duration<_Rep1, _Period1>(lhs)) * T(rhs).count());
}

template<typename _Rep1, typename _Period1,
         typename _Rep2>
duration<typename std::common_type<_Rep1,_Rep2>::type, _Period1>
constexpr operator/(duration<_Rep1,_Period1> const& lhs, _Rep2 const& rhs)
{
    using T = typename common_type<
        duration<_Rep1,_Period1>,
        duration<_Rep2,_Period1>>::type;

    return T(T(duration<_Rep1, _Period1>(lhs)) / T(rhs).count());
}

template<typename _Rep1, typename _Period1,
         typename _Rep2, typename _Period2>
typename std::common_type<_Rep1,_Rep2>::type
constexpr operator/(duration<_Rep1,_Period1> const& lhs,
                    duration<_Rep2,_Period2> const& rhs)
{
    using T = typename common_type<
        duration<_Rep1,_Period1>,
        duration<_Rep2,_Period1>>::type;

    return T(T(lhs).count() / T(rhs).count());
}

template<typename _Rep1, typename _Period1, typename _Rep2>
duration<typename std::common_type<_Rep1,_Rep2>::type, _Period1>
constexpr operator%(duration<_Rep1, _Period1> const& d, _Rep2 const& s)
{

}

template<typename _Rep1, typename _Period1,
         typename _Rep2, typename _Period2>
typename std::common_type<duration<_Rep1,_Period1>,
    duration<_Rep2,_Period2>>::type
constexpr operator%(duration<_Rep1,_Period1> const& lhs,
                    duration<_Rep2,_Period2> const& rhs)
{
    using T = typename common_type<
        duration<_Rep1,_Period1>,
        duration<_Rep2,_Period1>>::type;

    return T(T(lhs).count() % T(rhs).count());
}

// Not quite standard, drops floating point support
template<typename _ToDuration, typename _Rep, typename _Period>
constexpr _ToDuration duration_cast(duration<_Rep,_Period> const& rhs)
{
    using __conv = typename std::ratio_divide<
        _Period, typename _ToDuration::period>;

    // To see in debugger
    auto __n = __conv::num;
    auto __d = __conv::den;
    auto __r = typename _ToDuration::rep(intmax_t(rhs.count()) * __n / __d);

    return _ToDuration(__r);
}

template<typename _Clock, typename _Duration = typename _Clock::duration>
class time_point
{
public:
    using clock = _Clock;
    using duration = _Duration;
    using rep = typename _Duration::rep;
    using period = typename _Duration::period;

    constexpr time_point()
        : __point(0)
    {
    }

    constexpr explicit time_point(duration const& rhs)
        : __point(rhs)
    {
    }

    template<typename _Duration2>
    constexpr time_point(time_point<_Clock, _Duration2> const& rhs)
        : __point(duration_cast<duration,
                typename _Duration2::rep,
                typename _Duration2::period>(rhs))

    {
    }

    constexpr duration time_since_epoch() const
    {
        return __point;
    }

    template<typename _Rep, typename _Period>
    time_point operator+(chrono::duration<_Rep, _Period> const& rhs) const
    {
        return time_point(__point + rhs);
    }

    duration operator-(time_point const& rhs)
    {
        return duration(__point - rhs.__point);
    }

    duration operator-(duration const& rhs) const
    {
        return __point + rhs;
    }

    bool operator<(time_point const& rhs) const
    {
        return __point.count() < rhs.__point.count();
    }

    bool operator<=(time_point const& rhs) const
    {
        return __point.count() <= rhs.__point.count();
    }

    bool operator==(time_point const& rhs) const
    {
        return __point.count() == rhs.__point.count();
    }

    bool operator!=(time_point const& rhs) const
    {
        return __point.count() != rhs.__point.count();
    }

    bool operator>=(time_point const& rhs) const
    {
        return __point.count() >= rhs.__point.count();
    }

    bool operator>(time_point const& rhs) const
    {
        return __point.count() > rhs.__point.count();
    }

    static constexpr time_point<_Clock, _Duration> max();
    static constexpr time_point min();

private:
    duration __point;
};

using nanoseconds  = duration<uint64_t, nano>;
using microseconds = duration<uint64_t, micro>;
using milliseconds = duration<uint64_t, milli>;
using seconds      = duration<uint64_t>;
using minutes      = duration<uint64_t, ratio<60>>;
using hours        = duration<uint64_t, ratio<3600>>;
//c++20 using days         = duration<int64_t, ratio<86400>>;
//c++20 using weeks        = duration<int64_t, ratio<604800>>;
//c++20 using months       = duration<int64_t, ratio<2629746>>;
//c++20 using years        = duration<int64_t, ratio<31556952>>;

// Never treat as floating point
template<typename Rep>
struct treat_as_floating_point : integral_constant<bool, false> {};

// system_clock ticks in nanoseconds, epoch is 1970/01/01-00:00:00 UTC
class system_clock
{
public:
    using duration = nanoseconds;
    using period = duration::period;
    using rep = duration::rep;
    using time_point = std::chrono::time_point<system_clock>;

    static constexpr bool is_steady()
    {
        return false;
    }

    static time_point now();

private:
    static int64_t __epoch;
};

// steady_clock ticks in nanoseconds, epoch is datetime at boot
class steady_clock
{
public:
    using duration = nanoseconds;
    using period = duration::period;
    using rep = duration::rep;
    using time_point = std::chrono::time_point<steady_clock>;

    static constexpr bool is_steady()
    {
        return true;
    }

    static time_point now();
};

using high_resolution_clock = steady_clock;

// System time time_point in specified units since epoch
template<typename _Duration>
using sys_time = time_point<system_clock, _Duration>;

// System time time_point in seconds since epoch
using sys_seconds = sys_time<seconds>;

// System time time_point in days since epoch
//using sys_days = sys_time<days>;

template<typename _Clock, typename _Duration>
constexpr time_point<_Clock, _Duration> time_point<_Clock, _Duration>::max()
{
    return time_point(std::numeric_limits<std::chrono::
                      high_resolution_clock::duration::rep>::max());
}


__END_NAMESPACE // chrono
__END_NAMESPACE_STD

// Both clock implementations use int64_t nanosecond period
extern template class std::chrono::duration<int64_t, std::nano>;
extern template class std::chrono::time_point<std::chrono::system_clock>;
extern template class std::chrono::time_point<std::chrono::steady_clock>;
