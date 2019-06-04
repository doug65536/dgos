#pragma once
#include <stdlib.h>

#ifdef __cplusplus

__BEGIN_NAMESPACE_STD

struct nothrow_t {
    explicit nothrow_t() = default;
};
enum class align_val_t : size_t {};
extern nothrow_t const nothrow;

__END_NAMESPACE_STD

void *operator new(size_t size);
void *operator new(size_t size, void *p) noexcept;
void *operator new(size_t size, std::align_val_t alignment);
void *operator new(size_t size, std::nothrow_t const&);
void *operator new(size_t size, std::align_val_t alignment,
                   std::nothrow_t const&);
void *operator new(size_t size);
void operator delete(void *block) noexcept;
void operator delete(void *block, unsigned long size) noexcept;
void operator delete[](void *block) noexcept;
void operator delete[](void *block, unsigned int) noexcept;


#endif
