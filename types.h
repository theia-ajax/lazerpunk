#pragma once

#include <cstring>
#include <cstdint>
#include <cassert>
#include "enumflag.h"

#define ASSERT(expr) assert(expr)

constexpr int mod(int a, int b)
{
	int r = a % b;
	return r < 0 ? r + b : r;
}
