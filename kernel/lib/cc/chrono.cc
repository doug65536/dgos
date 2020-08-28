#include "chrono.h"
#include "export.h"

template class ext::chrono::duration<int64_t, ext::nano>;
template class ext::chrono::time_point<ext::chrono::system_clock>;
template class ext::chrono::time_point<ext::chrono::steady_clock>;

__BEGIN_NAMESPACE_EXT

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

__END_NAMESPACE_EXT
