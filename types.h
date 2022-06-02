#pragma once

#include <cstring>
#include <cstdint>
#include <cassert>
#include "enumflag.h"
#include "stringid.h"

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

struct Vec2
{
	float x = 0.0f, y = 0.0f;
};

constexpr Vec2 operator+(const Vec2& a, const Vec2& b) { return Vec2{ a.x + b.x, a.y + b.y }; }
constexpr Vec2 operator-(const Vec2& a, const Vec2& b) { return Vec2{ a.x - b.x, a.y - b.y }; }
constexpr Vec2 operator-(const Vec2& a) { return Vec2{ -a.x, -a.y }; }
constexpr Vec2 operator*(const Vec2& a, float b) { return Vec2{ a.x * b, a.y * b }; }
constexpr Vec2 operator/(const Vec2& a, float b) { return Vec2{ a.x / b, a.y / b }; }
constexpr bool operator==(const Vec2& a, const Vec2& b) { return a.x == b.x && a.y == b.y; }  // NOLINT(clang-diagnostic-float-equal)
constexpr bool operator!=(const Vec2& a, const Vec2& b) { return !(a == b); }  // NOLINT(clang-diagnostic-float-equal)

struct Color
{
	uint8_t r, g, b, a;
};

struct Camera
{
	Vec2 position;
	Vec2 extents;
	float pixelsToUnit = 16.0f;
};

namespace camera
{
	inline Vec2 WorldToScreen(const Camera& camera, Vec2 world) { return world * camera.pixelsToUnit - camera.position * camera.pixelsToUnit; }
}

struct SDL_Renderer;

struct DrawContext
{
	SDL_Renderer* renderer;
};