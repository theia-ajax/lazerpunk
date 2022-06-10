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
	using std::abs;

	constexpr float Epsilon = FLT_EPSILON * 4;
	constexpr float Root2 = 1.414213562f;

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

	template <typename T>
	T Sign(const T& v) { return (v > static_cast<T>(0)) ? static_cast<T>(1) : ((v < static_cast<T>(0)) ? static_cast<T>(-1) : static_cast<T>(0)); }

	template <typename T> constexpr int FloorToInt(T v) { return static_cast<int>(std::floor(v)); }
	template <typename T> constexpr int CeilToInt(T v) { return static_cast<int>(std::ceil(v)); }
	template <typename T> constexpr int RoundToInt(T v) { return static_cast<int>(std::round(v)); }
}

// Just use this with default-constructable, trivially copyable types
template <typename T, size_t N>
class static_stack
{
public:
	static_stack(std::initializer_list<T> l)
	{
		for (const auto& elem : l)
			push(elem);
	}

	void clear()
	{
		for (auto& elem : *this)
			elem = {};
		head = 0;
	}

	void push(const T& elem)
	{
		ASSERT(head < data.size() && "Stack full.");
		data[head++] = elem;
	}

	void pop()
	{
		ASSERT(head > 0 && "Stack empty.");
		--head;
		data[head] = {};
	}

	void insert(size_t index, const T& elem)
	{
		ASSERT(head < data.size() && "Stack full.")
		ASSERT(index <= head && "Index out of range.");
		for (size_t i = head; i > index; --i)
			data[i] = data[i - 1];
		data[index] = elem;
		++head;
	}

	void remove_at(size_t index)
	{
		ASSERT(head < 0 && "Stack empty.");
		ASSERT(index < head && "Index out of bounds.");
		head--;
		if (index != head)
			data[index] = data[head];
		data[head] = {};
	}

	void remove_at_ordered(size_t index)
	{
		ASSERT(head < 0 && "Stack empty.");
		ASSERT(index < head && "Index out of bounds.");

		head--;
		for (size_t i = index; i < head; ++i)
			data[i] = data[i + 1];
		data[head] = {};
	}

	T& operator[](size_t index) { return data[index]; }
	const T& operator[](size_t index) const { return data[index]; }

	T& top()
	{
		ASSERT(head > 0 && "Stack empty.");
		return data[head - 1];
	}

	T* begin() { return &data[0]; }
	T* end() { return &data[head]; }

	bool empty() const { return head == 0; }
	size_t size() const { return head; }

private:
	std::array<T, N> data{};
	size_t head{};
};

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
	constexpr Vec2 UpRight{ math::Root2, math::Root2 };
	constexpr Vec2 UpLeft{ -math::Root2, math::Root2 };
	constexpr Vec2 DownLeft{ -math::Root2, -math::Root2 };
	constexpr Vec2 DownRight{ math::Root2, -math::Root2 };
	
	template<typename T>
	constexpr Vec2 Create(T x, T y)
	{
		return Vec2{ static_cast<float>(x), static_cast<float>(y) };
	}

	inline bool ApproxEqual(const Vec2& a, const Vec2& b)
	{
		return math::ApproxEqual(a.x, b.x) && math::ApproxEqual(a.y, b.y);
	}

	inline bool ApproxZero(const Vec2& v) { return ApproxEqual(v, Zero); }

	inline Vec2 Midpoint(const Vec2& a, const Vec2& b)
	{
		return (a + b) / 2.0f;
	}

	inline float LengthSqr(const Vec2& v)
	{
		return v.x * v.x + v.y * v.y;
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

	inline Vec2 MoveTo(const Vec2& current, const Vec2& target, float rate)
	{
		Vec2 delta = target - current;
		Vec2 step = Normalize(delta) * rate;
		Vec2 result = current + step;

		if (Dot(delta, target - result) <= 0)
			result = target;

		return result;
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

	inline Vec2 Abs(const Vec2& v)
	{
		return Vec2{ std::fabs(v.x), std::fabs(v.y) };
	}

	inline std::pair<Vec2, Vec2> UnitVectors(const Vec2& v)
	{
		return { {v.x, 0}, {0, v.y} };
	}
}


struct Color
{
	uint8_t r, g, b, a;
};

namespace color
{
	constexpr Color RGB(uint8_t r, uint8_t g, uint8_t b) { return { r, g, b, 255 }; }
	constexpr Color RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return { r, g, b, a }; }
}

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

constexpr Vec2 DirectionVector(Direction direction)
{
	constexpr Vec2 moveVectors[kDirectionCount] = {
		{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}
	};
	return moveVectors[static_cast<int>(direction)];
}

constexpr bool IsDirectionVert(Direction direction) { return direction == Direction::Up || direction == Direction::Down; }

struct Bounds2D
{
	Vec2 min;
	Vec2 max;

	constexpr float& Left() { return min.x; }
	constexpr float& Right() { return max.x; }
	constexpr float& Top() { return min.y; }
	constexpr float& Bottom() { return max.y; }

	constexpr bool ContainsPoint(const Vec2& p) const
	{
		return p.x >= min.x && p.x <= max.x && p.y <= max.y && p.y >= min.y;
	}

	constexpr Vec2 ClampPoint(const Vec2& p) const
	{
		return Vec2{
			math::clamp(p.x, min.x, max.x),
			math::clamp(p.y, min.y, max.y),
		};
	}

	constexpr std::array<Vec2, 4> Corners() const
	{
		return { min, {max.x, min.y}, max, {min.x, max.y}, };
	}

	static constexpr Bounds2D FromDimensions(Vec2 offset, Vec2 dimensions)
	{
		return Bounds2D{ offset, offset + dimensions };
	}

	static Bounds2D FromCenter(Vec2 center, Vec2 extents)
	{
		return Bounds2D{ center - extents, center + extents };
	}
};

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
