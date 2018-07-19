#pragma once
#include "type_traits.h"

template<typename T>
constexpr bool enable_bitwise(T)
{
    return false;
}

template<typename E>
constexpr E& operator|=(E& lhs, E rhs)
{
    static_assert(enable_bitwise(E{}),
                  "Bitwise operators not enabled for this type");

    using underlying = typename std::underlying_type<E>::type;

    return lhs = static_cast<E>(static_cast<underlying>(lhs) |
            static_cast<underlying>(rhs));
}

template<typename E>
constexpr E& operator&=(E& lhs, E rhs)
{
    static_assert(enable_bitwise(E{}),
                  "Bitwise operators not enabled for this type");

    using underlying = typename std::underlying_type<E>::type;

    return lhs = static_cast<E>(static_cast<underlying>(lhs) &
                          static_cast<underlying>(rhs));
}

template<typename E>
constexpr E& operator^=(E& lhs, E rhs)
{
    static_assert(enable_bitwise(E{}),
                  "Bitwise operators not enabled for this type");

    using underlying = typename std::underlying_type<E>::type;

    return lhs = static_cast<E>(static_cast<underlying>(lhs) ^
                          static_cast<underlying>(rhs));
}

template<typename E>
constexpr E operator~(E rhs)
{
    static_assert(enable_bitwise(E{}),
                  "Bitwise operators not enabled for this type");

    using underlying = typename std::underlying_type<E>::type;

    return static_cast<E>(~static_cast<underlying>(rhs));
}

template<typename E>
constexpr E operator|(E lhs, E rhs)
{
    static_assert(enable_bitwise(E{}),
                  "Bitwise operators not enabled for this type");

    return lhs |= rhs;
}

template<typename E>
constexpr E operator&(E lhs, E rhs)
{
    static_assert(enable_bitwise(E{}),
                  "Bitwise operators not enabled for this type");

    return lhs &= rhs;
}

template<typename E>
constexpr E operator^(E lhs, E rhs)
{
    static_assert(enable_bitwise(E{}),
                  "Bitwise operators not enabled for this type");

    return lhs ^= rhs;
}
