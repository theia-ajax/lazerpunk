#pragma once

#include <type_traits>
#include <array>

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

namespace flags
{
	template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
	constexpr bool Test(T flags, T test)
	{
		return (flags & test) == test;
	}

	template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
	T Set(T& flags, T set, bool value)
	{
		if (value)
			flags |= set;
		else
			flags &= ~set;
		return flags;
	}
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr T& operator++(T& value)
{
	value = static_cast<T>(static_cast<std::underlying_type_t<T>>(value) + 1);
	return value;
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr T operator++(T& value, int i)
{
	T result = value;
	value = static_cast<T>(static_cast<std::underlying_type_t<T>>(value) + (i) ? i : 1);
	return result;
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr T& operator--(T& value)
{
	value = static_cast<T>(static_cast<std::underlying_type_t<T>>(value) - 1);
	return value;
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int>* = nullptr>
constexpr T operator--(T& value, int i)
{
	T result = value;
	value = static_cast<T>(static_cast<std::underlying_type_t<T>>(value) - (i) ? i : 1);
	return result;
}

template <class T, typename I>
struct enum_array : std::array<T, static_cast<std::underlying_type_t<I>>(I::Count)>
{
	T& operator[](const I& i) { return std::array<T, static_cast<std::underlying_type_t<I>>(I::Count)>::operator[](static_cast<std::underlying_type_t<I>>(i)); }
	const T& operator[](const I& i) const { return std::array<T, static_cast<std::underlying_type_t<I>>(I::Count)>::operator[](static_cast<std::underlying_type_t<I>>(i)); }
};