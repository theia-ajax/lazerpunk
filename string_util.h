#pragma once

#include <algorithm>
#include <functional>
#include <string>

namespace string_util
{
	using char_pred = std::function<bool(unsigned char)>;

	inline void ltrim(std::string& s, char_pred pred)
	{
		s.erase(s.begin(), std::ranges::find_if_not(s, pred));
	}

	inline void rtrim(std::string& s, char_pred pred)
	{
		s.erase(std::find_if_not(s.rbegin(), s.rend(), pred).base(), s.end());
	}

	inline void trim(std::string& s, char_pred pred)
	{
		ltrim(s, pred); rtrim(s, pred);
	}

	// trim from start (in place)
	inline void ltrim(std::string& s)
	{
		ltrim(s, [](auto ch) {return std::isspace(ch); });
	}

	// trim from end (in place)
	inline void rtrim(std::string& s)
	{
		rtrim(s, [](auto ch) {return std::isspace(ch); });
	}

	// trim from both ends (in place)
	inline void trim(std::string& s)
	{
		ltrim(s); rtrim(s);
	}

	inline void ltrim(std::string& s, char trimChar)
	{
		ltrim(s, [trimChar](auto ch) { return ch == trimChar; });
	}

	inline void rtrim(std::string& s, char trimChar)
	{
		ltrim(s, [trimChar](auto ch) { return ch == trimChar; });
	}

	inline void trim(std::string& s, char trimChar)
	{
		ltrim(s, trimChar); rtrim(s, trimChar);
	}
}
