#pragma once
#include "types.h"
#include "cxxstring.h"

__BEGIN_DECLS

#ifdef USE_RETPOLINE
_no_plt
#endif
bool mm_copy_user(void *dst, void const *src, size_t size);

/// If src == nullptr, clears destination, otherwise copies from src
/// If src != nullptr, behaves exactly like strncpy
#ifdef USE_RETPOLINE
_no_plt
#endif
bool mm_copy_user_str(char *dst, char const *src, size_t size);

#ifdef USE_RETPOLINE
_no_plt
#endif
intptr_t mm_lenof_user_str(char const *src, size_t max_size);

_const
bool mm_is_user_range(void const *buf, size_t size);

_const
bool mm_max_user_len(void const *buf);

using mm_copy_string_result_t = std::pair<std::string, bool>;

mm_copy_string_result_t mm_copy_user_string(
        char const *user_src, size_t max_size);

__END_DECLS
