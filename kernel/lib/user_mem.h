#pragma once
#include "types.h"
#include "cxxstring.h"
#include "syscall/sys_limits.h"

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

int mm_compare_exchange_user(volatile int *dest, int *expect, int replacement);

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

struct user_str_t {
    struct truncate_t {};

    user_str_t(char const *user_str);
    user_str_t(char const *user_str, size_t truncate_len, truncate_t);
    user_str_t(user_str_t const&) = delete;
    user_str_t operator=(user_str_t const&) = delete;

    // Returns true if holding a valid null terminated string
    operator bool() const
    {
        return lenof_str >= 0;
    }

    // Return a pointer to the kernel copy of the string if there is
    // a string present, otherwise returns nullptr
    operator char const *() const
    {
        return lenof_str >= 0
                ? reinterpret_cast<char const *>(data.data)
                : nullptr;
    }

    // Returns the length of the string, excluding the null terminator
    // if there is a string present, otherwise returns 0
    size_t len() const
    {
        return lenof_str >= 0 ? lenof_str : 0;
    }

    // Returns the errno of the user-to-kernel transfer result
    // Successful use returns errno_t::OK
    errno_t err() const
    {
        return lenof_str < 0 ? errno_t(-lenof_str) : errno_t::OK;
    }

    // Returns the negated integer errno of the user-to-kernel transfer result
    // Successful use returns errno_t::OK
    int err_int() const
    {
        return lenof_str < 0 ? lenof_str : int(errno_t::OK);
    }

    void set_err(errno_t err)
    {
        lenof_str = -intptr_t(err);
    }

    static constexpr size_t max_sz = PATH_MAX;

    // Doubles as length storage when >= 0
    // otherwise holds negated errno value
    intptr_t lenof_str;
    typename std::aligned_storage<max_sz, sizeof(char const*)>::type data;
};
