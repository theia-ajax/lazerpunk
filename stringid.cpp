#include "stringid.h"

#define STRPOOL_U32 uint32_t
#define STRPOOL_U64 uint64_t
#include "strpool.h"

namespace
{
	struct StrPool
	{
		StrPool() = default;
		explicit StrPool(const strpool_config_t& config);
		~StrPool();
		StrPool(const StrPool& other) = delete;
		StrPool(StrPool&& other) = delete;
		StrPool& operator=(const StrPool& other) = delete;
		StrPool& operator=(StrPool&& other) = delete;

		StrId::RawType Inject(const char* str, size_t len);
		const char* CStr(const StrId& strId) const;

		strpool_t pool{};
	};

	StrPool::StrPool(const strpool_config_t& config)
	{
		strpool_init(&pool, &config);
	}

	StrPool::~StrPool()
	{
		strpool_term(&pool);
	}

	StrId::RawType StrPool::Inject(const char* str, size_t len)
	{
		return static_cast<StrId::RawType>(strpool_inject(&pool, str, static_cast<int>(len)));
	}

	const char* StrPool::CStr(const StrId& strId) const
	{
		return (strId.IsEmpty()) ? "" : strpool_cstr(&pool, strId.RawValue());
	}

	StrPool g_StringPool;
	StrPool* g_StringPoolPtr = nullptr;

	constexpr strpool_config_t GetConfig()
	{
		strpool_config_t config = strpool_default_config;

		if constexpr (sizeof(StrId::RawType) == sizeof(uint32_t))
		{
			config.counter_bits = 8;
			config.index_bits = 24;
		}
		else if constexpr (sizeof(StrId::RawType) == sizeof(uint64_t))
		{
			config.counter_bits = 16;
			config.index_bits = 48;
		}

		return config;
	}

	StrPool& GetStrPool()
	{
		if (g_StringPoolPtr == nullptr)
		{
			g_StringPoolPtr = new (&g_StringPool) StrPool(GetConfig());
		}
		return *g_StringPoolPtr;
	}
}

StrId::StrId(const char* str) : StrId(str, strlen(str)) {}
StrId::StrId(const char* str, size_t len) : rawValue(GetStrPool().Inject(str, len))
{
#ifdef _DEBUG
	this->stringValue = CStr();  // NOLINT(cppcoreguidelines-prefer-member-initializer)
#endif
}
StrId::StrId(const std::string& str) : StrId(str.c_str(), str.length()) {}

const char* StrId::CStr() const { return GetStrPool().CStr(*this); }

