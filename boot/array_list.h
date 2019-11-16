#pragma once
#include "types.h"
#include "malloc.h"
#include "likely.h"

template<typename T, typename S = size_t>
class array_list
{
public:
    array_list()
        : items(nullptr)
        , count(0)
        , capacity(0)
    {
    }

    array_list(array_list&& rhs)
        : items(rhs.items)
        , count(rhs.count)
        , capacity(rhs.capacity)
    {
        rhs.items = nullptr;
        rhs.count = 0;
        rhs.capacity = 0;
    }

    ~array_list();

    using pointer = T*;
    using const_pointer = T const*;
    using reference = T&;
    using const_reference = T const&;
    using iterator = T*;
    using const_iterator = T const*;

    bool add(T &&item);

    template<typename... Args>
    bool add(Args&& ...args);

    void clear();

    void remove_at(S index);
    _always_inline T &item(S index) { return items[index]; }
    _always_inline T &operator[](S index) { return item(index); }
    _always_inline T const& operator[](S index) const { return item(index); }
    _always_inline T* begin() { return items; }
    _always_inline T* end() { return items + count; }
    _always_inline T const* begin() const { return items; }
    _always_inline T const* end() const { return items + count; }
    _always_inline T const* cbegin() const { return begin(); }
    _always_inline T const* cend() const { return end(); }

private:
    T *items;
    S count;
    S capacity;
};

template<typename T, typename S>
template<typename... Args>
bool array_list<T, S>::add(Args&& ...args)
{
    return add(T(static_cast<Args&&>(args)...));
}

template<typename T, typename S>
array_list<T, S>::~array_list()
{
    for (S i = 0; i < count; ++i)
        items[i].~T();
    free(items);
}

template<typename T, typename S>
bool array_list<T, S>::add(T&& item)
{
    S new_capacity = capacity ? (capacity << 1) : 4;
    T *new_items = malloc(sizeof(T) * new_capacity);
    if (unlikely(!new_items))
        return false;
    for (S i = 0; i < count; ++i) {
        new_items[i] = static_cast<T&&>(items[i]);
        items[i].~T();
    }
    free(items);
    items = new_items;
    capacity = new_capacity;
    return true;
}

template<typename T, typename S>
void array_list<T, S>::clear()
{
    for (S i = 0; i < count; ++i)
        items[i].~T();
    count = 0;
}

