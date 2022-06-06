#include "stringid.h"

#define STRPOOL_U32 uint32_t
#define STRPOOL_U64 uint64_t
#include "strpool.h"

namespace
{
	struct CollatedStrPool
	{
		const strpool_t* pool;
		char* collated;
		int count;

		explicit CollatedStrPool(const strpool_t* pool)
			: pool(pool)
		{
			collated = strpool_collate(pool, &count);
		}

		~CollatedStrPool()
		{
			strpool_free_collated(pool, collated);
		}
	};

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
		CollatedStrPool Collate() const;
		constexpr const strpool_t* Raw() const { return &pool; }

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

	CollatedStrPool StrPool::Collate() const
	{
		return CollatedStrPool(&pool);
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

StringReport StrId::QueryStringReport()
{
	CollatedStrPool collated = GetStrPool().Collate();
	const strpool_t* pool = GetStrPool().Raw();


	StringReport result{};

	result.entryCapacity = pool->entry_capacity;
	result.entryCount = pool->entry_count;
	result.blockSize = pool->block_size;
	result.blockCount = pool->block_count;
	result.blockCapacity = pool->block_capacity;

	const char* current = collated.collated;
	while (current && result.strings.size() < static_cast<unsigned>(collated.count))
	{
		result.strings.emplace_back(current);
		size_t len = strlen(current) + 1;
		current += len;
	}

	return result;
}
