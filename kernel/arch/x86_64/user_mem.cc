#include "user_mem.h"
#include "cpu/control_regs.h"
#include "cpu/except.h"
#include "numeric_limits.h"
#include "export.h"
#include "mm.h"

//
// mm_copy_user
extern "C" int nofault_memcpy(void *dest, void const *src, size_t size);
extern "C" int nofault_memset(void *dest, int value, size_t size);
extern "C" int nofault_strncpy(char *dest, char const *value, size_t size);
extern "C" ptrdiff_t nofault_strnlen(char const *s, size_t max_size);
extern "C" int nofault_compare_exchange_32(
        int volatile *dest, int *expect, int replacement);

extern "C" ptrdiff_t nofault_offsetof(char const *s, int value, size_t size);

bool mm_copy_user(void *dst, const void *src, size_t size)
{
    if (src)
        return nofault_memcpy(dst, src, size) >= 0;

    return nofault_memset(dst, 0, size) >= 0;
}

//
// mm_copy_user_str

ptrdiff_t mm_copy_user_str(char *dst, char const *src, size_t max_size)
{
    return nofault_strncpy(dst, src, max_size);
}

//
// mm_lenof_user_str

intptr_t mm_lenof_user_str(char const *src, size_t max_size)
{
    return nofault_strnlen(src, max_size);
}

bool mm_is_user_range(void const *buf, size_t size)
{
    return uintptr_t(buf) >= 0x400000 &&
            (uintptr_t(buf) < 0x7FFFFFFFFFFF) &&
            (uintptr_t(buf) + size) <= 0x7FFFFFFFFFFF;
}

size_t mm_max_user_len(void const *buf)
{
    return (uintptr_t(buf) >= 0x400000 &&
            uintptr_t(buf) < 0x7FFFFFC00000)
            ? (char*)0x7FFFFFC00000 - (char*)buf
            : 0;
}

mm_copy_string_result_t mm_copy_user_string(
        char const *user_src, size_t max_size)
{
    mm_copy_string_result_t result{};

    size_t absolute_max = mm_max_user_len(user_src);

    // Clamp max to be within the boundary of user address space
    if (unlikely(max_size > absolute_max))
        max_size = absolute_max;

    intptr_t len1 = mm_lenof_user_str(user_src, max_size);

    if (len1 >= 0 && result.first.reserve(len1 + 1) &&
            result.first.resize(len1) &&
            mm_copy_user(result.first.data(), user_src, len1 + 1))
    {
        // Did not fault
        intptr_t len2 = strlen(result.first.data());

        // Double-check that length stayed consistent
        if (likely(len2 >= 0 && len2 == len1))
            // Okay good
            result.second = true;
        else {
            // Nice try buddy, the string length changed. Now I'm mad.
            result.first.clear();
        }
    }

    return result;
}


int mm_compare_exchange_user(int volatile *dest, int *expect, int replacement)
{
    return nofault_compare_exchange_32(dest, expect, replacement);
}
