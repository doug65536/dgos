#include "type_traits.h"
#include "assert.h"

template<typename... _Types>
union union_storage
{
};

template<typename _T>
union union_storage<_T>
{
    typename ext::aligned_storage<sizeof(_T), alignof(_T)>::type current;
};

template<typename _T, typename... _Types>
union union_storage<_T, _Types...>
{
    union_storage<_Types...> other;
    typename ext::aligned_storage<sizeof(_T), alignof(_T)>::type current;
};

template<typename _Base, typename... _Types>
class union_wrapper
{
    using _Storage = union_storage<_Types...>;
    alignas(_Types...) _Storage __storage;
    bool __constructed;

public:
    union_wrapper()
        : __constructed(false)
    {
    }

    ~union_wrapper()
    {
        destruct();
    }

    template<typename U, typename... Args>
    void construct(Args&&... args) {
        assert(!__constructed);
        new (&__storage) U(ext::forward<Args>(args)...);
        __constructed = true;
    }

    void destruct()
    {
        if (__constructed) {
            reinterpret_cast<_Base*>(&__storage)->~_Base();
            __constructed = false;
        }
    }

    _Base* operator->()
    {
        return reinterpret_cast<_Base*>(&__storage);
    }

    _Base const *operator->() const
    {
        return reinterpret_cast<_Base*>(&__storage);
    }
};

