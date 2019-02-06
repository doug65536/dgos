#include "iocp.h"
#include "export.h"

#pragma GCC visibility push(default)
template class std::pair<errno_t, size_t>;
template class dgos::__basic_iocp_error_success_t<std::pair<errno_t, size_t>>;
template class dgos::basic_iocp_t<std::pair<errno_t, size_t>,
        dgos::__basic_iocp_error_success_t<std::pair<errno_t, size_t>>>;
template class dgos::basic_blocking_iocp_t<
        std::pair<errno_t, size_t>,
        dgos::__basic_iocp_error_success_t<std::pair<errno_t, size_t>>>;
#pragma GCC visibility pop
