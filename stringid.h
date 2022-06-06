#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct StringReport
{
	int blockSize;
	int blockCapacity;
	int blockCount;
	int entryCapacity;
	int entryCount;
	std::vector<std::string> strings{};
};

class StrId
{
public:
	using RawType = uint32_t;

	static constexpr RawType kEmptyRaw = static_cast<RawType>(0);

	constexpr StrId() : StrId(kEmptyRaw) {}
	constexpr explicit StrId(RawType rawId);
	StrId(const char* str);
	StrId(const char* str, size_t len);
	StrId(const std::string& str);

	const char* CStr() const;
	constexpr bool IsEmpty() const { return RawValue() == kEmptyRaw; }
	constexpr RawType RawValue() const { return rawValue; };

	static StringReport QueryStringReport();

public:
	static const StrId kEmpty;

private:
	RawType rawValue;

#ifdef _DEBUG
	const char* stringValue;
#endif
};

constexpr StrId::StrId(RawType rawId)
	: rawValue(rawId)
#ifdef _DEBUG
	, stringValue(nullptr)
#endif
{}

inline constexpr StrId StrId::kEmpty{ kEmptyRaw };

inline bool operator==(const StrId& a, const StrId& b) { return a.RawValue() == b.RawValue(); }
inline bool operator!=(const StrId& a, const StrId& b) { return a.RawValue() != b.RawValue(); }
inline bool operator<(const StrId& a, const StrId& b) { return a.RawValue() < b.RawValue(); }

template <>
struct std::hash<StrId>
{
	std::size_t operator()(const StrId& s) const noexcept
	{
		return std::hash<StrId::RawType>{}(s.RawValue());
	}
};

