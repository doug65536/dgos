#pragma once
#include "types.h"
#include "export.h"

#ifndef PAGE_SCALE
#define PAGE_SCALE  12
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE   4096L
#endif

#define _MALLOC_OVERHEAD    16

#ifdef __cplusplus
struct bad_alloc
{
};
#endif

__BEGIN_DECLS

void malloc_startup(void *p);

_malloc _assume_aligned(16) _alloc_size(1, 2)
KERNEL_API_BUILTIN void *calloc(size_t num, size_t size);

_malloc _assume_aligned(16) _alloc_size(1)
KERNEL_API_BUILTIN void *malloc(size_t size);

_assume_aligned(16) _alloc_size(2)
KERNEL_API_BUILTIN void *realloc(void *p, size_t new_size);

KERNEL_API_BUILTIN void free(void *p);

KERNEL_API bool malloc_validate(bool dump);

_malloc _assume_aligned(16)
KERNEL_API char *strdup(char const *s);

KERNEL_API int strtoi(char const *str, char **end, int base);
KERNEL_API long strtol(char const *str, char **end, int base);
KERNEL_API unsigned long strtoul(char const *str, char **end, int base);
KERNEL_API long long strtoll(char const *str, char **end, int base);
KERNEL_API unsigned long long strtoull(char const *str, char **end, int base);

int atoi(char const *str);
int atol(char const *str);
int atoll(char const *str);

__END_DECLS

_malloc _assume_aligned(16)
KERNEL_API_BUILTIN void *operator new(size_t size);

_const
KERNEL_API_BUILTIN void *operator new(size_t size, void *p) noexcept;

_malloc _assume_aligned(16)
KERNEL_API_BUILTIN void *operator new[](size_t size);

KERNEL_API_BUILTIN void operator delete(void *block) noexcept;
KERNEL_API_BUILTIN void operator delete[](void *block) noexcept;
KERNEL_API_BUILTIN void operator delete[](void *block, unsigned int);

__BEGIN_NAMESPACE_STD

struct KERNEL_API nothrow_t {
    explicit nothrow_t() = default;
};
enum class align_val_t : size_t {};
KERNEL_API extern nothrow_t const nothrow;

__END_NAMESPACE_STD

__BEGIN_NAMESPACE_EXT

struct nothrow_t {
    explicit nothrow_t() = default;
};
extern KERNEL_API nothrow_t const nothrow;

__END_NAMESPACE_EXT

__BEGIN_NAMESPACE_STD

template<typename _T>
constexpr _T abs(_T __rhs)
{
    return __rhs >= 0 ? __rhs : -__rhs;
}

__END_NAMESPACE_STD

KERNEL_API_BUILTIN void* operator new[](size_t count,
        std::align_val_t alignment);
KERNEL_API void* operator new[](size_t count, std::align_val_t alignment,
    std::nothrow_t const&);
KERNEL_API void *operator new(size_t size, ext::nothrow_t const&) noexcept;
KERNEL_API void *operator new[](size_t size, ext::nothrow_t const&) noexcept;

KERNEL_API_BUILTIN void operator delete(
        void *block, unsigned long size) noexcept;

class KERNEL_API zero_init_t
{
public:
    void *operator new(size_t size, ext::nothrow_t const&) noexcept
    {
        return calloc(1, size);
    }

    void operator delete(void *p) noexcept
    {
        free(p);
    }
};
//void operator delete(void *block) throw();
//void operator delete[](void *block) throw();
