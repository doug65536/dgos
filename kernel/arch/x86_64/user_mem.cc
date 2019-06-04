#include "user_mem.h"
#include "cpu/control_regs.h"
#include "cpu/except.h"
#include "numeric_limits.h"
#include "export.h"

//
// mm_copy_user

bool mm_copy_user_generic(void *dst, void const *src, size_t size)
{
    __try {
        if (src)
            memcpy(dst, src, size);
        else
            memset(dst, 0, size);
    } __catch {
        return false;
    }

    return true;
}

bool mm_copy_user_smap(void *dst, void const *src, size_t size)
{
    cpu_stac();
    bool result = mm_copy_user_generic(dst, src, size);
    cpu_clac();
    return result;
}

//
// mm_copy_user_str

static bool mm_copy_user_str_generic(
        char *dst, char const *src, size_t max_size)
{
    __try {
        if (src)
            strncpy(dst, src, max_size);
        else
            memset(dst, 0, max_size);
    } __catch {
        return false;
    }

    return true;
}

static bool mm_copy_user_str_smap(char *dst, char const *src, size_t size)
{
    cpu_stac();
    bool result = mm_copy_user_str_generic(dst, src, size);
    cpu_clac();
    return result;
}

//
// mm_lenof_user_str

intptr_t mm_lenof_user_str_generic(char const *src, size_t max_size)
{
    __try {
        if (unlikely(max_size > size_t(std::numeric_limits<intptr_t>::max())))
            return -1;

        for (uintptr_t len = 0; len < max_size; ++len) {
            if (src[len] == 0)
                return intptr_t(len);
        }
    } __catch {
    }
    return -1;
}

intptr_t mm_lenof_user_str_smap(char const *src, size_t max_size)
{
    cpu_stac();
    intptr_t result = mm_lenof_user_str_generic(src, max_size);
    cpu_clac();
    return result;
}

typedef bool (*mm_copy_user_fn)(
        void *dst, void const *src, size_t size);
typedef bool (*mm_copy_user_str_fn)(
        char *dst, char const *src, size_t size);
typedef intptr_t (*mm_lenof_user_str_fn)(
        char const *src, size_t max_size);

extern "C" mm_copy_user_fn mm_copy_user_resolver()
{
    if (cpuid_has_smap())
        return mm_copy_user_smap;
    return mm_copy_user_generic;
}

extern "C" mm_copy_user_str_fn mm_copy_user_str_resolver()
{
    if (cpuid_has_smap())
        return mm_copy_user_str_smap;
    return mm_copy_user_str_generic;
}

extern "C" mm_lenof_user_str_fn mm_lenof_user_str_resolver()
{
    if (cpuid_has_smap())
        return mm_lenof_user_str_smap;
    return mm_lenof_user_str_generic;
}

_ifunc_resolver(mm_copy_user_resolver)
EXPORT bool mm_copy_user(void *dst, void const *src, size_t size);

_ifunc_resolver(mm_copy_user_str_resolver)
EXPORT bool mm_copy_user_str(char *dst, char const *src, size_t size);

_ifunc_resolver(mm_lenof_user_str_resolver)
EXPORT intptr_t mm_lenof_user_str(char const *src, size_t max_size);

EXPORT bool mm_is_user_range(void const *buf, size_t size)
{
    return uintptr_t(buf) >= 0x400000 &&
            (uintptr_t(buf) < 0x7FFFFFFFFFFF) &&
            (uintptr_t(buf) + size) <= 0x7FFFFFFFFFFF;
}

EXPORT bool mm_max_user_len(void const *buf)
{
    return (uintptr_t(buf) >= 0x400000 &&
            uintptr_t(buf) < 0x7FFFFFFFFFFF)
            ? (char*)0x800000000000 - (char*)buf
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

    if (len1 >= 0 && result.first.resize(len1 + 1) &&
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
