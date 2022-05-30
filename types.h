#pragma once

#include <cstring>
#include <cstdint>
#include <cassert>
#include "enumflag.h"

namespace internal { void PrintAssert(const char* function, int lineNum, const char* exprStr); }

#define ASSERT_ALWAYS(expr) \
	{ \
		static bool triggered = false; \
		if (!(expr) && !triggered) { \
			triggered = true; \
			internal::PrintAssert(__FUNCTION__, __LINE__, #expr); \
			__debugbreak(); \
		} \
	}

#ifdef _DEBUG
#define ASSERT(expr) ASSERT_ALWAYS(expr)
#else
#define ASSERT(expr)
#endif

constexpr int mod(int a, int b)
{
	int r = a % b;
	return r < 0 ? r + b : r;
}
