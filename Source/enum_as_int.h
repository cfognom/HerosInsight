#pragma once
// AI Generated, supposed to make enums act like integers

template <typename E>
constexpr typename std::enable_if<std::is_enum<E>::value, E>::type
operator|(E lhs, E rhs)
{
    using underlying = typename std::underlying_type<E>::type;
    return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <typename E>
constexpr typename std::enable_if<std::is_enum<E>::value, E>::type
operator&(E lhs, E rhs)
{
    using underlying = typename std::underlying_type<E>::type;
    return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <typename E>
constexpr typename std::enable_if<std::is_enum<E>::value, E>::type
operator|=(E &lhs, E rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

template <typename E>
constexpr typename std::enable_if<std::is_enum<E>::value, E>::type
operator&=(E &lhs, E rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

template <typename E>
constexpr typename std::enable_if<std::is_enum<E>::value, bool>::type
operator!(E val)
{
    using underlying = typename std::underlying_type<E>::type;
    return static_cast<underlying>(val) == 0;
}

template <typename E>
constexpr typename std::enable_if<std::is_enum<E>::value, bool>::type
operator==(E lhs, std::underlying_type_t<E> rhs)
{
    return static_cast<std::underlying_type_t<E>>(lhs) == rhs;
}

template <typename E>
constexpr typename std::enable_if<std::is_enum<E>::value, bool>::type
operator!=(E lhs, std::underlying_type_t<E> rhs)
{
    return !(lhs == rhs);
}

template <typename E>
constexpr typename std::enable_if<std::is_enum<E>::value, bool>::type
operator==(std::underlying_type_t<E> lhs, E rhs)
{
    return rhs == lhs;
}

template <typename E>
constexpr typename std::enable_if<std::is_enum<E>::value, bool>::type
operator!=(std::underlying_type_t<E> lhs, E rhs)
{
    return !(lhs == rhs);
}
