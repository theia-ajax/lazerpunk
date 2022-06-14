#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <numeric>

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
template <typename T, int N>
class static_stack
{
	using ArrayType = std::array<T, N>;

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
		m_head = 0;
	}

	void push(const T& elem)
	{
		ASSERT(!full() && "Stack full.");
		m_mem[m_head++] = elem;
	}

	void pop()
	{
		ASSERT(!empty() && "Stack empty.");
		--m_head;
		m_mem[m_head] = {};
	}

	void insert(int index, const T& elem)
	{
		ASSERT(!full() && "Stack full.")
		ASSERT(index <= m_head && "Index out of range.");
		for (int i = m_head; i > index; --i)
			m_mem[i] = m_mem[i - 1];
		m_mem[index] = elem;
		++m_head;
	}

	void remove_at(int index)
	{
		ASSERT(!empty() && "Stack empty.");
		ASSERT(index < m_head && "Index out of bounds.");
		m_head--;
		if (index != m_head)
			m_mem[index] = m_mem[m_head];
		m_mem[m_head] = {};
	}

	void remove_at_ordered(int index)
	{
		ASSERT(!empty() && "Stack empty.");
		ASSERT(index < m_head && "Index out of bounds.");

		m_head--;
		for (int i = index; i < m_head; ++i)
			m_mem[i] = m_mem[i + 1];
		m_mem[m_head] = {};
	}

	void remove_element(T* elem)
	{
		ASSERT(!empty() && "Stack empty.");
		ptrdiff_t elemIdx = elem - data();
		ASSERT(elemIdx >= 0 && elemIdx < m_head && "Pointer not in range.");
		remove_at(static_cast<int>(elemIdx));
	}

	void remove_element_ordered(T* elem)
	{
		ASSERT(!empty() && "Stack empty.");
		ptrdiff_t elemIdx = elem - data();
		ASSERT(elemIdx >= 0 && elemIdx < m_head && "Pointer not in range.");
		remove_at_ordered(static_cast<int>(elemIdx));
	}

	T& operator[](int index) { return m_mem[index]; }
	const T& operator[](int index) const { return m_mem[index]; }

	T& top()
	{
		ASSERT(!empty() && "Stack empty.");
		return m_mem[m_head - 1];
	}

	typename ArrayType::iterator begin() { return m_mem.begin(); }
	typename ArrayType::iterator end() { return m_mem.end(); }
	typename ArrayType::iterator rbegin() { return m_mem.rbegin(); }
	typename ArrayType::iterator rend() { return m_mem.rend(); }

	constexpr const T* data() const { return m_mem.data(); }

	bool empty() const { return m_head <= 0; }
	bool full() const { return m_head >= capacity(); }
	int size() const { return m_head; }
	constexpr int capacity() const { return N; }

private:
	ArrayType m_mem{};
	int m_head{};
};

namespace types
{
	template <typename T, int N, typename Func>
	int erase_if(static_stack<T, N>& stack, Func&& predicate)
	{
		int start = stack.size();
		for (int i = 0; i < stack.size(); ++i)
		{
			if (predicate(stack[i]))
			{
				stack.remove_at_ordered(i);
			}
		}
		return start - stack.size();
	}
}

template <typename T, int N>
struct ring_buf
{
	using ArrayType = std::array<T, N>;

	constexpr int next_index(int idx, int delta) const { return mod(idx + delta, ssize()); }
	constexpr int ssize() const { return static_cast<int>(m_mem.size()); }
	constexpr size_t size() const { return m_mem.size(); }
	constexpr int index() const { return m_index; }

	constexpr T* current() { return &m_mem[m_index]; }

	constexpr T* next()
	{
		T* ret = &m_mem[m_index];
		m_index = next_index(m_index, 1);
		return ret;
	}

	constexpr T* next(T* ptr)
	{
		ptrdiff_t diff = ptr - m_mem.data();
		ASSERT(diff >= 0 && diff < m_mem.size() && "Pointer not in ring buffer.");
		int idx = next_index(static_cast<int>(diff), 1);
		return &m_mem[idx];
	}

	constexpr T* prev()
	{
		T* ret = &m_mem[m_index];
		m_index = next_index(m_index, -1);
		return ret;
	}

	constexpr T* prev(T* ptr)
	{
		ptrdiff_t diff = ptr - m_mem.data();
		ASSERT(diff >= 0 && diff < m_mem.size() && "Pointer not in ring buffer.");
		int idx = next_index(static_cast<int>(diff), -1);
		return &m_mem[idx];
	}

	T& operator[](int index)
	{
		ASSERT(index >= 0 && index < m_mem.size() && "Invalid index.");
		return m_mem[index];
	}

	const T& operator[](int index) const
	{
		ASSERT(index >= 0 && index < m_mem.size() && "Invalid index.");
		return m_mem[index];
	}

	typename ArrayType::iterator begin() { return m_mem.begin(); }
	typename ArrayType::iterator end() { return m_mem.end(); }
	typename ArrayType::iterator rbegin() { return m_mem.rbegin(); }
	typename ArrayType::iterator rend() { return m_mem.rend(); }

private:
	ArrayType m_mem{};
	int m_index = 0;
};

namespace types
{
	template <typename T, int N>
	T average(ring_buf<T, N>& rbuf)
	{
		return std::accumulate(rbuf.begin(), rbuf.end(), static_cast<T>(0)) / static_cast<T>(N);
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

	Vec2 Center() const { return vec2::Midpoint(min, max); }
	Vec2 Size() const { return { max.x - min.x, max.y - min.y }; };
	Vec2 HalfSize() const { return Size() / 2; }

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

	static Bounds2D FromCenter(Vec2 center, Vec2 halfSize)
	{
		if (halfSize.x < 0) halfSize.x = 0;
		if (halfSize.y < 0) halfSize.y = 0;
		return Bounds2D{ center - halfSize, center + halfSize };
	}

	static Bounds2D Grow(const Bounds2D& bounds, Vec2 halfGrowSize)
	{
		return FromCenter(bounds.Center(), bounds.HalfSize() + halfGrowSize);
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

