#include "iocp.h"

template struct ext::pair<errno_t, size_t>;
template class dgos::__basic_iocp_error_success_t<ext::pair<errno_t, size_t>>;
template class dgos::basic_iocp_t<ext::pair<errno_t, size_t>,
        dgos::__basic_iocp_error_success_t<ext::pair<errno_t, size_t>>>;
template class dgos::basic_blocking_iocp_t<
        ext::pair<errno_t, size_t>,
        dgos::__basic_iocp_error_success_t<ext::pair<errno_t, size_t>>>;
