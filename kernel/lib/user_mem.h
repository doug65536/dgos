#pragma once
#include "types.h"
#include "cxxstring.h"

__BEGIN_DECLS

_use_result
bool mm_copy_user(void *dst, void const *src, size_t size);

/// If src == nullptr, clears destination, otherwise copies from src
/// Returns the length of the string if the copy was successful
/// Returns -1 on fault
/// Returns -2 if input string too long
_use_result
ptrdiff_t mm_copy_user_str(char *dst, char const *src, size_t size);

intptr_t mm_lenof_user_str(char const *src, size_t max_size);

_use_result
_const
bool mm_is_user_range(void const *buf, size_t size);

_use_result
_const
size_t mm_max_user_len(void const *buf);


__END_DECLS

using mm_copy_string_result_t = std::pair<std::string, bool>;

mm_copy_string_result_t mm_copy_user_string(
        char const *user_src, size_t max_size);

