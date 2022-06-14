#pragma once

#include <array>
#include <bit>
#include <cstdint>

template <size_t N, typename T = uint64_t>
struct bitfield
{
	static_assert(N > 0);
	static constexpr size_t CHUNK_SIZE = sizeof(T) * 8;
	static constexpr size_t CHUNK_COUNT = (N % CHUNK_SIZE == 0) ? N / CHUNK_SIZE : N / CHUNK_SIZE + 1;

	bitfield() : chunks() {}
	explicit bitfield(T chunk) : chunks() { chunks[0] = chunk; }
	explicit bitfield(const std::array<T, N>& values) : chunks(values){}

	void reset()
	{
		chunks = {};
	}

	constexpr auto chunk_mask(int bit) -> std::pair<std::reference_wrapper<T>, T>
	{
		int chunkIndex = bit / CHUNK_SIZE;
		int chunkBit = bit - (chunkIndex * CHUNK_SIZE);
		T chunkMask = static_cast<T>(1) << chunkBit;
		return std::make_pair(std::ref(chunks[chunkIndex]), chunkMask);
	}

	void set(int bit, bool value)
	{
		auto [chunk, mask] = chunk_mask(bit);

		if (value)
			chunk |= mask;
		else
			chunk &= ~mask;
	}

	bool test(int bit)
	{
		auto [chunk, mask] = chunk_mask(bit);
		return (chunk & mask) == mask;
	}

	bool empty() const { return lowest() < 0; }

	int lowest() const
	{
		for (size_t i = 0; i < CHUNK_COUNT; ++i)
		{
			if (chunks[i] != 0)
				return std::countr_zero(chunks[i]) + static_cast<int>(i * CHUNK_SIZE);
		}
		return -1;
	}

	int highest() const
	{
		for (size_t i = CHUNK_COUNT - 1; i >= 0; --i)
		{
			if (chunks[i] != 0)
				return (CHUNK_SIZE - std::countl_zero(chunks[i]) - 1) + static_cast<int>(i * CHUNK_SIZE);
		}
		return -1;
	}

	bool operator[](int bit) const { return test(bit); }

	bitfield<N, T>& operator&=(const bitfield<N, T>& other) { *this = *this & other; return *this; }
	bitfield<N, T>& operator|=(const bitfield<N, T>& other) { *this = *this | other; return *this; }
	bitfield<N, T>& operator^=(const bitfield<N, T>& other) { *this = *this ^ other; return *this; }

	constexpr size_t hash() const
	{
		size_t res = 17;
		for (size_t c = 0; c < CHUNK_COUNT; ++c)
			res *= 31 + std::hash<T>()(chunks[c]);
		return res;
	}

	std::array<T, CHUNK_COUNT> chunks{};
};

template <size_t N, typename T = uint64_t>
constexpr bool operator==(const bitfield<N, T>& a, const bitfield<N, T>& b)
{
	for (size_t i = 0; i < bitfield<N, T>::CHUNK_COUNT; ++i)
	{
		if (a.chunks[i] != b.chunks[i])
			return false;
	}
	return true;
}

template <size_t N, typename T = uint64_t>
constexpr bool operator!=(const bitfield<N, T>& a, const bitfield<N, T>& b)
{
	for (size_t i = 0; i < bitfield<N, T>::CHUNK_COUNT; ++i)
	{
		if (a.chunks[i] != b.chunks[i])
			return true;
	}
	return false;
}

template <size_t N, typename T = uint64_t>
constexpr bitfield<N, T> operator&(const bitfield<N, T>& a, const bitfield<N, T>& b)
{
	bitfield<N, T> result;
	for (size_t i = 0; i < bitfield<N, T>::CHUNK_COUNT; ++i)
		result.chunks[i] = a.chunks[i] & b.chunks[i];
	return result;
}

template <size_t N, typename T = uint64_t>
constexpr bitfield<N, T> operator|(const bitfield<N, T>& a, const bitfield<N, T>& b)
{
	bitfield<N, T> result;
	for (size_t i = 0; i < bitfield<N, T>::CHUNK_COUNT; ++i)
		result.chunks[i] = a.chunks[i] | b.chunks[i];
	return result;
}

template <size_t N, typename T = uint64_t>
constexpr bitfield<N, T> operator^(const bitfield<N, T>& a, const bitfield<N, T>& b)
{
	bitfield<N, T> result;
	for (size_t i = 0; i < bitfield<N, T>::CHUNK_COUNT; ++i)
		result.chunks[i] = a.chunks[i] ^ b.chunks[i];
	return result;
}

template <size_t N, typename T = uint64_t>
constexpr bool operator<(const bitfield<N, T>& a, const bitfield<N, T>& b)
{
	for (size_t i = 0; i < bitfield<N, T>::CHUNK_COUNT; ++i)
	{
		auto chunkA = a.chunks[i]; auto chunkB = b.chunks[i];
		if (chunkA < chunkB)
			return true;
		if (chunkB < chunkA)
			return false;
	}
	return false;
}

template <size_t N>
struct std::formatter<bitfield<N, uint64_t>> : std::formatter<std::string>
{
	auto format(bitfield<N, uint64_t> bf, format_context& ctx) const
	{
		if constexpr (bitfield<N, uint64_t>::CHUNK_COUNT == 1)
			return std::formatter<string>::format(std::format("{:016x}", bf.chunks[0]), ctx);
		else if constexpr (bitfield<N, uint64_t>::CHUNK_COUNT == 2)
			return std::formatter<string>::format(std::format("{:016x}{:016x}", bf.chunks[1], bf.chunks[0]), ctx);
	}
};
