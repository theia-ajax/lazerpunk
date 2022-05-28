#pragma once

#include <type_traits>


template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr T operator|(T a, T b) { return T(static_cast<std::underlying_type_t<T>>(a) | static_cast<std::underlying_type_t<T>>(b)); }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
T& operator|=(T& a, T b) { return (T&)(reinterpret_cast<std::underlying_type_t<T>&>(a) |= static_cast<std::underlying_type_t<T>>(b)); }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr T operator&(T a, T b) { return T(static_cast<std::underlying_type_t<T>>(a) & static_cast<std::underlying_type_t<T>>(b)); }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
T& operator&=(T& a, T b) { return (T&)(reinterpret_cast<std::underlying_type_t<T>&>(a) &= static_cast<std::underlying_type_t<T>>(b)); }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr T operator^(T a, T b) { return T(static_cast<std::underlying_type_t<T>>(a) ^ static_cast<std::underlying_type_t<T>>(b)); }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
T& operator^=(T& a, T b) { return (T&)(reinterpret_cast<std::underlying_type_t<T>&>(a) ^= static_cast<std::underlying_type_t<T>>(b)); }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr T operator~(T a) { return T(~(static_cast<std::underlying_type_t<T>>(a))); }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr bool operator==(T a, T b) { return static_cast<std::underlying_type_t<T>&>(a) == static_cast<std::underlying_type_t<T>&>(b); }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr bool operator!=(T a, T b) { return static_cast<std::underlying_type_t<T>&>(a) != static_cast<std::underlying_type_t<T>&>(b); }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr bool operator!(T a) { return a == static_cast<T>(0); }

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr bool FlagTest(T flags, T test)
{
	return (flags & test) == test;
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
T FlagSet(T& flags, T set, bool value)
{
	if (value)
		flags |= set;
	else
		flags &= ~set;
	return flags;
}
