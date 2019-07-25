#include "chrono.h"
#include "export.h"

template class std::chrono::duration<int64_t, std::nano>;
template class std::chrono::time_point<std::chrono::system_clock>;
template class std::chrono::time_point<std::chrono::steady_clock>;

__BEGIN_NAMESPACE_STD

// Adjustment factor in ns to go from steady_clock to system_clock time_point
int64_t chrono::system_clock::__epoch;

EXPORT chrono::steady_clock::time_point chrono::steady_clock::now()
{
    return time_point(time_ns());
}

EXPORT chrono::system_clock::time_point chrono::system_clock::now()
{
    return time_point(time_ns() + __epoch);
}

__END_NAMESPACE_STD
