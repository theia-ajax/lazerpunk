#pragma once

#include <cstring>
#include <cstdint>
#include <algorithm>
#include <cmath>
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

namespace math
{
	using std::min;
	using std::max;
	using std::lerp;
	using std::expf;
	using std::clamp;

	constexpr float Epsilon = FLT_EPSILON * 4;

	inline bool ApproxEqual(float a, float b)
	{
		return fabsf(a - b) <= Epsilon;
	}

	inline bool ApproxZero(float f)
	{
		return ApproxEqual(f, 0.0f);
	}

	inline float MoveTo(float current, float target, float rate)
	{
		if (current < target)
			return min(current + rate, target);
		else
			return max(current - rate, target);
	}

	inline float Damp(float current, float target, float lambda, float dt)
	{
		return lerp(current, target, 1.0f - expf(-lambda * dt));
	}
}

struct Vec2
{
	float x = 0.0f;
	float y = 0.0f;
};

constexpr Vec2 operator+(const Vec2& a, const Vec2& b) { return Vec2{ a.x + b.x, a.y + b.y }; }
inline Vec2 operator-(const Vec2& a, const Vec2& b) { return Vec2{ a.x - b.x, a.y - b.y }; }
inline Vec2 operator-(const Vec2& a) { return Vec2{ -a.x, -a.y }; }
inline Vec2 operator*(const Vec2& a, float b) { return Vec2{ a.x * b, a.y * b }; }
inline Vec2 operator*(const Vec2& a, const Vec2& b) { return Vec2{ a.x * b.x,a.y * b.y }; }
inline Vec2 operator/(const Vec2& a, float b) { return Vec2{ a.x / b, a.y / b }; }
inline Vec2 operator/(const Vec2& a, const Vec2& b) { return Vec2{ a.x / b.x, a.y / b.y }; }
inline bool operator==(const Vec2& a, const Vec2& b) { return a.x == b.x && a.y == b.y; }  // NOLINT(clang-diagnostic-float-equal)
inline bool operator!=(const Vec2& a, const Vec2& b) { return !(a == b); }  // NOLINT(clang-diagnostic-float-equal)

namespace vec2
{
	constexpr Vec2 Zero{ 0, 0 };
	constexpr Vec2 One{ 1, 1 };
	constexpr Vec2 Half{ 0.5f, 0.5f };
	constexpr Vec2 UnitX{ 1, 0 };
	constexpr Vec2 UnitY{ 0, 1 };

	template<typename T>
	constexpr Vec2 Create(T x, T y)
	{
		return Vec2{ static_cast<float>(x), static_cast<float>(y) };
	}

	inline Vec2 Midpoint(const Vec2& a, const Vec2& b)
	{
		return (a + b) / 2.0f;
	}

	inline float Length(const Vec2& v)
	{
		return sqrtf(v.x * v.x + v.y * v.y);
	}

	inline Vec2 Normalize(const Vec2& v)
	{
		float l = Length(v);
		return (math::ApproxZero(l)) ? Zero : v / l;
	}

	inline float Dot(const Vec2& a, const Vec2& b)
	{
		return a.x * b.x + a.y * b.y;
	}

	inline Vec2 Lerp(const Vec2& a, const Vec2& b, float t)
	{
		return Vec2{ math::lerp(a.x, b.x, t), math::lerp(a.y, b.y, t) };
	}

	inline Vec2 Damp(const Vec2& a, const Vec2& b, const Vec2& lambda, float dt)
	{
		return Vec2{
			math::Damp(a.x, b.x, lambda.x, dt),
			math::Damp(a.y, b.y, lambda.y, dt),
		};
	}

	inline Vec2 Damp(const Vec2& a, const Vec2& b, float lambda, float dt)
	{
		return Vec2{
			math::Damp(a.x, b.x, lambda, dt),
			math::Damp(a.y, b.y, lambda, dt),
		};
	}
}


struct Color
{
	uint8_t r, g, b, a;
};

struct SDL_Renderer;


enum class Direction
{
	Invalid,
	Left,
	Right,
	Up,
	Down,
	Count,
};

constexpr int kDirectionCount = static_cast<int>(Direction::Count);

constexpr Vec2 DirectionVelocity(Direction direction)
{
	constexpr Vec2 moveVectors[kDirectionCount] = {
		{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}
	};
	return moveVectors[static_cast<int>(direction)];
}

struct Camera
{
	Vec2 position;
	Vec2 extents;
	float scale = 16.0f;
};

namespace camera
{
	inline Vec2 WorldScaleToScreen(const Camera& camera, Vec2 world) { return world * camera.scale; }
	inline Vec2 WorldToScreen(const Camera& camera, Vec2 world) { return world * camera.scale - camera.position; }
}

struct GameTime
{
	GameTime(double elapsed, double delta) : elapsedSec(elapsed), deltaSec(delta) {}
	float t() const { return static_cast<float>(elapsedSec); }
	float dt() const { return static_cast<float>(deltaSec); }
private:
	const double elapsedSec;
	const double deltaSec;
};