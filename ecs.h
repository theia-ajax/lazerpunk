#pragma once

// Adapted from the example provided on this blog post
// https://austinmorlan.com/posts/entity_component_system


#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>
#include <set>
#include <stack>
#include <unordered_map>

#include "bitfield.h"
#include "types.h"

#ifndef ENABLE_ECS_LOGGING
#define ENABLE_ECS_LOGGING 0
#endif

#if ENABLE_ECS_LOGGING
#include "debug.h"

namespace ecs
{
	using debug::Log;
}
#else
namespace ecs { template <typename... Args> void Log(std::string_view fmt, Args&&... args) {} }
#endif

class World;
using Entity = int32_t;
constexpr Entity kMaxEntities = 4096;
constexpr Entity kInvalidEntity = 0;

using ComponentType = uint8_t;
constexpr ComponentType kMaxComponents = 64;

struct Prefab {};

template <typename T>
struct Reject { using Component = T; };

template <class T, template <class...> class Template>
struct is_specialization : std::false_type {};

template <template <class...> class Template, class... Args>
struct is_specialization<Template<Args...>, Template> : std::true_type {};

template <typename T>
struct is_reject_component : is_specialization<T, Reject> {};

template <typename T>
inline constexpr bool is_reject_component_v = is_reject_component<T>::value;

template <typename... T>
struct component_reject_filter;

template <>
struct component_reject_filter<>
{
	using type = std::tuple<>;
};

template <typename T>
struct component_reject_filter<T>
{
	using type = typename std::conditional_t<not is_reject_component_v<std::decay_t<T>>, std::tuple<T>, std::tuple<>>;
};

template <typename T, typename... Ts>
struct component_reject_filter<T, Ts...>
{
	using type = decltype(std::tuple_cat(component_reject_filter<T>::type(), component_reject_filter<Ts...>::type()));
};

template <typename... T>
using component_reject_filter_t = typename component_reject_filter<T...>::type;

template <typename T>
using component_ref_vector_t = std::vector<std::reference_wrapper<T>>;

template <typename... T>
struct component_ref_vector_reject_filter;

template <>
struct component_ref_vector_reject_filter<>
{
	using type = std::tuple<>;
};

template <typename T>
struct component_ref_vector_reject_filter<T>
{
	using type = typename std::conditional_t<not is_reject_component_v<T>, std::tuple<component_ref_vector_t<T>>, std::tuple<>>;
};

template <typename T, typename... Ts>
struct component_ref_vector_reject_filter<T, Ts...>
{
	using type = decltype(std::tuple_cat(component_ref_vector_reject_filter<T>::type(), component_ref_vector_reject_filter<Ts...>::type()));
};

template <typename... T>
using component_ref_vector_reject_filter_t = typename component_ref_vector_reject_filter<T...>::type;

// Signatures represent which 
struct Signature
{
	using Layer = bitfield<kMaxComponents>;

	Layer require;
	Layer reject;

	void reset() { require.reset(); reject.reset(); }
	bool Matches(const Signature& other) const
	{
		return (require & other.require) == require &&
			(reject & other.require).empty();
	}

	Signature& operator|=(const Signature& other)
	{
		require |= other.require;
		reject |= other.reject;
		return *this;
	}
};

inline bool operator==(const Signature& a, const Signature& b) { return a.require == b.require && a.reject == b.reject; }
inline bool operator<(const Signature& a, const Signature& b)
{
	return (a.require < b.require) || ((a.require == b.require) && a.reject < b.reject);
}

template<>
struct std::hash<Signature>
{
	size_t operator()(const Signature& signature) const noexcept
	{
		size_t res = 17;
		res = res * 31 + signature.require.hash();
		res = res * 31 + signature.reject.hash();
		return res;
	}
};

template<>
struct std::formatter<Signature> : std::formatter<std::string>
{
	auto format(Signature s, format_context& ctx) const
	{
		size_t hash = std::hash<Signature>()(s);
		return formatter<string>::format(std::format("{}", hash), ctx);
	}
};

void LogSignature(const World& world, const char* label, Signature signature);
void LogCompareSignatures(const World& world, const char* label1, Signature signature1, const char* label2, Signature signature2);

// Tracks available entity indices and signatures of active entities
class EntityManager
{
public:
	EntityManager(World& world)
		: world(world)
	{
		for (Entity entity = kMaxEntities - 1; entity >= 1; --entity)
			availableEntities.push(entity);
	}

	Entity CreateEntity()
	{
		ASSERT(!availableEntities.empty() && "Max entities reached.");

		Entity entity = availableEntities.top();
		availableEntities.pop();

		activeEntities.emplace(entity);

		return entity;
	}

	template <int N>
	std::array<Entity, N> CreateEntities()
	{
		std::array<Entity, N> entities;
		for (int i = 0; i < N; ++i)
		{
			entities[i] = CreateEntity();
		}
		return entities;
	}

	void DestroyEntity(Entity entity)
	{
		ecs::Log("[EntityManager] DestroyEntity {}", entity);
		ASSERT(entity < kMaxEntities && "Invalid entity.");

		signatures[entity].reset();
		availableEntities.push(entity);
		activeEntities.erase(entity);
	}

	void SetSignature(Entity entity, Signature signature)
	{
		ASSERT(entity < kMaxEntities && "Invalid entity.");

		signatures[entity] = signature;
	}

	Signature GetSignature(Entity entity) const
	{
		ASSERT(entity < kMaxEntities && "Invalid entity.");

		return signatures[entity];
	}

	Entity GetEntityCount() const
	{
		return static_cast<Entity>(activeEntities.size());
	}

	constexpr const std::set<Entity>& GetActiveEntities() const { return activeEntities; }

	void GetEntitiesMatchingSignature(Signature signature, std::vector<Entity>& entities) const
	{
		entities.clear();
		for (Entity entity : activeEntities)
		{
			if (Signature entitySignature = GetSignature(entity); signature.Matches(entitySignature))
			{
				ecs::Log("New Query {} matches Entity {}", signature, entity);
				LogCompareSignatures(world, "New Query Match", signature, "Entity", entitySignature);
				entities.emplace_back(entity);
			}
		}
	}

private:
	std::stack<Entity> availableEntities{};
	std::set<Entity> activeEntities;
	std::array<Signature, kMaxEntities + 1> signatures{};
	World& world;
};

class IComponentArray
{
public:
	virtual ~IComponentArray() = default;
	virtual void OnEntityDestroyed(Entity entity) = 0;

	virtual void* TryGetUntypedComponentPtr(Entity entity) = 0;
	virtual size_t GetComponentSize() = 0;
	virtual void* InsertUntyped(Entity entity, const void* source, size_t size) = 0;
};

template <typename T>
class ComponentArray final : public IComponentArray
{
public:
	void* InsertUntyped(Entity entity, const void* source, size_t sourceSize) override
	{
		ASSERT(!entityToIndexMap.contains(entity) && "Component already added to entity.");

		size_t newIndex = size++;
		entityToIndexMap[entity] = newIndex;
		indexToEntityMap[newIndex] = entity;

		std::memcpy(&componentArray[newIndex], source, sourceSize);
		return &componentArray[newIndex];
	}

	T& Insert(Entity entity, const T& component)
	{
		ASSERT(!entityToIndexMap.contains(entity) && "Component already added to entity.");

		size_t newIndex = size++;
		entityToIndexMap[entity] = newIndex;
		indexToEntityMap[newIndex] = entity;
		componentArray[newIndex] = component;

		return componentArray[newIndex];
	}

	T& GetDummyComponent() { return componentArray[0]; }

	void Remove(Entity entity)
	{
		ecs::Log("ComponentArray<{}> Remove {}", typeid(T).name() + 7, entity);


		ASSERT(entityToIndexMap.contains(entity) && "Component missing for entity.");

		// swap last element into removed index
		// update mapping such that the removed index points to the swapped in entity
		// and the swapped entity points to the removed index

		size_t indexOfRemovedEntity = entityToIndexMap[entity];
		size_t indexOfLastElement = size - 1;
		componentArray[indexOfRemovedEntity] = componentArray[indexOfLastElement];

		Entity entityOfLastElement = indexToEntityMap[indexOfLastElement];
		entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
		indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

		// remove the removed entity from the index mapping and remove the last element from the entity mapping
		entityToIndexMap.erase(entity);
		indexToEntityMap.erase(indexOfLastElement);

		size--;
	}

	bool Contains(Entity entity) const
	{
		return entityToIndexMap.contains(entity);
	}

	T& Get(Entity entity)
	{
		ASSERT(entityToIndexMap.contains(entity) && "Component missing for entity.");
		return componentArray[entityToIndexMap[entity]];
	}

	void OnEntityDestroyed(Entity entity) override
	{
		if (entityToIndexMap.contains(entity))
		{
			Remove(entity);
		}
	}

	void* TryGetUntypedComponentPtr(Entity entity) override
	{
		if (entityToIndexMap.contains(entity))
		{
			return &componentArray[entityToIndexMap[entity]];
		}
		return nullptr;
	}

	size_t GetComponentSize() override
	{
		return sizeof(T);
	}

private:
	std::array<T, kMaxEntities + 1> componentArray{};
	std::unordered_map<Entity, size_t> entityToIndexMap{};
	std::unordered_map<size_t, Entity> indexToEntityMap{};
	size_t size = 1;
};

class ComponentManager
{
	using ComponentId = intptr_t;

	template <typename T>
	static constexpr ComponentId GetComponentId()
	{
		return reinterpret_cast<ComponentId>(typeid(T).name());
	}

	template <typename T>
	static constexpr const char* GetComponentName()
	{
		return typeid(T).name();
	}

public:

	template <typename T>
	auto RegisterComponent() -> std::enable_if_t<std::is_trivially_copyable_v<T>, ComponentId>
	{
		ComponentId componentId = GetComponentId<T>();
		ASSERT(!componentTypes.contains(componentId) && "Component already registered.");
		componentTypes.insert({ componentId, nextComponentType });
		componentIds.insert({ nextComponentType, componentId });
		componentNames.insert({ nextComponentType, GetComponentName<T>() });
		componentArrays.insert({ componentId, std::make_shared<ComponentArray<T>>() });
		++nextComponentType;
		return componentId;
	}

	template <typename T>
	ComponentType GetComponentType() const
	{
		ComponentId componentId = GetComponentId<T>();
		ASSERT(componentTypes.contains(componentId) && "Component not registered.");
		return componentTypes.at(componentId);
	}

	const char* GetComponentTypeName(ComponentType componentType) const
	{
		ASSERT(componentNames.contains(componentType) && "Component type not in name table.");
		return componentNames.at(componentType) + 7;
	}

	template <typename T>
	T& AddComponent(Entity entity, const T& component)
	{
		return GetComponentArray<T>()->Insert(entity, component);
	}

	void* AddComponentUntyped(Entity entity, ComponentType componentType, const void* source, size_t size)
	{
		return GetUntypedComponentArray(componentType)->InsertUntyped(entity, source, size);
	}

	template <typename T>
	void RemoveComponent(Entity entity)
	{
		GetComponentArray<T>()->Remove(entity);
	}

	template <typename T>
	bool HasComponent(Entity entity)
	{
		return GetComponentArray<T>()->Contains(entity);
	}

	template <typename T>
	T& GetComponent(Entity entity)
	{
		return GetComponentArray<T>()->Get(entity);
	}

	auto TryGetComponent(Entity entity, ComponentType type) -> std::pair<void*, size_t>
	{
		if (componentIds.contains(type))
		{
			auto componentArray = GetUntypedComponentArray(type);
			return std::make_pair(componentArray->TryGetUntypedComponentPtr(entity), componentArray->GetComponentSize());
		}
		return std::make_pair(nullptr, 0);
	}

	void OnEntityDestroyed(Entity entity)
	{
		ecs::Log("[ComponentManager] OnEntityDestroyed {}", entity);
		for (const auto& component : componentArrays | std::views::values)
		{
			component->OnEntityDestroyed(entity);
		}
	}

	template <typename... Components>
	Signature BuildSignature() const
	{
		return BuildSignatureHelper<Components...>();
	}

	std::string BuildSignatureLayerString(Signature::Layer layer) const
	{
		std::string ret;
		ret.reserve(128);

		while (!layer.empty())
		{
			int typeIndex = layer.lowest();
			ComponentType ctype = static_cast<ComponentType>(typeIndex);
			ret += GetComponentTypeName(ctype);
			layer.set(typeIndex, false);
			if (!layer.empty())
				ret += ", ";
		}

		return ret;
	}

private:
	template <typename Head, typename... Tail>
	Signature BuildSignatureHelper() const
	{
		Signature signature;
		if constexpr (is_reject_component_v<Head>)
		{
			ComponentType ignoredType = GetComponentType<typename Head::Component>();
			signature.reject.set(ignoredType, true);
		}
		else
		{
			ComponentType componentType = GetComponentType<Head>();
			signature.require.set(componentType, true);
		}
		if constexpr (sizeof...(Tail) > 0)
		{
			signature |= BuildSignature<Tail...>();
		}
		return signature;
	}

private:
	std::unordered_map<ComponentId, ComponentType> componentTypes{};
	std::unordered_map<ComponentType, ComponentId> componentIds{};
	std::unordered_map<ComponentType, const char*> componentNames{};
	std::unordered_map<ComponentId, std::shared_ptr<IComponentArray>> componentArrays;
	ComponentType nextComponentType{};

	template <typename T>
	std::shared_ptr<ComponentArray<T>> GetComponentArray()
	{
		ComponentId componentId = GetComponentId<T>();
		ASSERT(componentTypes.contains(componentId) && "Component not registerd.");
		return std::static_pointer_cast<ComponentArray<T>>(componentArrays[componentId]);
	}

	std::shared_ptr<IComponentArray> GetUntypedComponentArray(ComponentType componentType)
	{
		ASSERT(componentIds.contains(componentType) && "Unable to find component for component type.");
		return componentArrays[componentIds[componentType]];
	}
};

class QueryManager;
using QueryId = int32_t;

struct QueryCallbacks
{
	std::function<void(Entity)> onEntityMatch{};
	std::function<void(Entity)> onEntityUnmatch{};

	void OnEntityMatch(Entity entity) const { if (onEntityMatch) onEntityMatch(entity); }
	void OnEntityUnmatch(Entity entity) const { if (onEntityUnmatch) onEntityUnmatch(entity); }
};

static constexpr bool DefaultQuerySort(const World& world, Entity a, Entity b) { return a < b; }

class QueryBase  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using SortPredicate = std::function<bool(World&, Entity, Entity)>;

	QueryBase() = delete;
	QueryBase(const QueryBase& other) = delete;
	QueryBase(QueryBase&& other) = delete;
	QueryBase operator=(const QueryBase& other) = delete;
	QueryBase operator=(QueryBase&& other) = delete;

	explicit QueryBase(QueryId _queryId, World* _world, Signature _signature, QueryCallbacks _callbacks, SortPredicate _sortPredicate)
		: queryId(_queryId)
		, signature(_signature)
		, callbacks(std::move(_callbacks))
		, sortPredicate(std::move(_sortPredicate))
		, world(_world) {}

	World& GetWorld() const { return *world; }
	const std::vector<Entity>& GetEntities() { return entities; }
	Signature GetSignature() const { return signature; }
	void OnEntityMatch(Entity entity) const { callbacks.OnEntityMatch(entity); }
	void OnEntityUnmatch(Entity entity) const { callbacks.OnEntityUnmatch(entity); }

	void InitializeEntityList(const EntityManager& entityManager)
	{
		ASSERT(entities.empty() && "Query already contained entities before initializing entity list");
		entityManager.GetEntitiesMatchingSignature(signature, entities);
		if (!entities.empty())
			ecs::Log("Initialized query and added {} entities to it.", entities.size());
		for (Entity addedEntity : entities)
			OnEntityMatch(addedEntity);
	}

	virtual void InsertLists(ptrdiff_t index, Entity entity) {}
	virtual void RemoveLists(ptrdiff_t index) {}

	void Insert(ptrdiff_t index, Entity entity)
	{
		entities.insert(entities.begin() + index, entity);
		InsertLists(index, entity);
	}

	void Remove(ptrdiff_t index)
	{
		entities.erase(entities.begin() + index);
		RemoveLists(index);
	}

	void AddEntity(Entity entity)
	{
		auto insertAt = std::ranges::upper_bound(entities, entity, SortCaller(sortPredicate, *world));
		auto index = std::distance(entities.begin(), insertAt);
		Insert(index, entity);
		OnEntityMatch(entity);
	}

	void RemoveEntity(Entity entity)
	{
		auto eraseAt = std::ranges::lower_bound(entities, entity, SortCaller(sortPredicate, *world));
		auto index = std::distance(entities.begin(), eraseAt);
		Remove(index);
		OnEntityUnmatch(entity);
	}

	int32_t GetEntityIndex(Entity entity)
	{
		auto search = std::ranges::lower_bound(entities, entity, SortCaller(sortPredicate, *world));
		if (search == entities.end())
			return -1;
		return static_cast<int32_t>(std::distance(entities.begin(), search));
	}

	Entity GetEntityAtIndex(int32_t index) const
	{
		ASSERT(index >= 0 && index < static_cast<int32_t>(entities.size()) && "Index out of query range.");
		return entities[index];
	}

	SortPredicate GetSortPredicate() { return sortPredicate; }
	std::function<bool(Entity, Entity)> GetPredicate() const { return [this](Entity a, Entity b) { return sortPredicate(GetWorld(), a, b); }; }

protected:
	QueryId queryId;
	std::vector<Entity> entities;
	Signature signature;
	QueryCallbacks callbacks;
	SortPredicate sortPredicate;

	struct SortCaller
	{
		SortCaller(SortPredicate pred, World& world) : pred(std::move(pred)), world(world) {}
		bool operator()(Entity a, Entity b) const { return pred(world, a, b); }
		SortPredicate pred;
		World& world;
	};
	bool SortPredicateForward(Entity a, Entity b) const { return sortPredicate(*world, a, b); }

private:
	World* world;
};




template <typename... Components>
class Query final : public QueryBase
{
public:
	using TypeSignature = component_reject_filter_t<Components...>;

	Query() = delete;
	Query(const Query& other) = delete;
	Query(Query&& other) = delete;
	Query operator=(const Query& other) = delete;
	Query operator=(Query&& other) = delete;

	explicit Query(QueryId _queryId, World* _world, Signature _signature, QueryCallbacks _callbacks, SortPredicate _sortPredicate)
		: QueryBase(_queryId, _world, _signature, _callbacks, _sortPredicate) { }

	auto GetArchetype(Entity entity) const
	{
		return GetWorld().template GetComponents<Components...>(entity);
	}

	template <typename T>
	auto GetComponentList() const -> std::enable_if_t<not is_reject_component_v<T> and std::disjunction_v<std::is_same<T, Components>...>, const std::vector<std::reference_wrapper<T>>&>
	{
		return std::get<component_ref_vector_t<T>>(componentLists);
	}

	auto GetComponentLists() const
	{
		return GetComponentListsHelper<Components...>();
	}

	void InsertLists(ptrdiff_t index, Entity entity) override;
	void RemoveLists(ptrdiff_t index) override;

private:
	template <typename T>
	auto GetFirstListHelper() const
	{
		if constexpr (is_reject_component_v<T>)
			return std::tuple<>();
		else
			return std::tuple<std::vector<std::reference_wrapper<T>>>(GetComponentList<T>());
	}

	template <typename Head, typename... Tail>
	auto GetComponentListsHelper() const
	{
		if constexpr (sizeof...(Tail) > 0)
			return std::tuple_cat(GetFirstListHelper<Head>(), GetComponentListsHelper<Tail...>());
		else
			return GetFirstListHelper<Head>();
	}

private:
	component_ref_vector_reject_filter_t<Components...> componentLists;
};

class QueryManager
{
public:
	explicit QueryManager(World& world) : world(world) {}

	template <class... Components> Query<Components...>* CreateQuery(QueryCallbacks callbacks, QueryBase::SortPredicate sortPredicate);
	QueryBase* GetQueryUntypedById(QueryId queryId) const;
	template <class... Components> Query<Components...>* GetQueryById(QueryId queryId);

	void OnEntitySignatureChanged(Entity entity, Signature newSignature, Signature oldSignature);
private:
	World& world;

	int32_t nextQueryId = 1;
	std::set<Signature> signatures;
	std::unordered_map<QueryId, std::unique_ptr<QueryBase>> queries;
	std::unordered_map<Signature, std::vector<QueryId>> queriesBySignature;
};

enum class SystemFlags
{
	None = 0,
	Monitor = 1 << 0,
	MonitorGlobalEntityDestroy = 1 << 1,
};

struct SystemBase  // NOLINT(cppcoreguidelines-special-member-functions)
{
	virtual ~SystemBase();

	World& GetWorld() const;

	friend class SystemManager;

	virtual void OnEntityAdded(Entity entity);
	virtual void OnEntityRemoved(Entity entity);
	virtual void OnRegistered();

	SystemFlags Flags() const;

private:
	World* world{};
	SystemFlags flags = SystemFlags::None;
};

inline SystemBase::~SystemBase() = default;
inline World& SystemBase::GetWorld() const { return *world; }
inline SystemFlags SystemBase::Flags() const { return flags; }
inline void SystemBase::OnEntityAdded(Entity entity) {}
inline void SystemBase::OnEntityRemoved(Entity entity) {}
inline void SystemBase::OnRegistered() {}

template <typename T, typename... Components>
struct System : SystemBase
{
	auto GetSystemQuery();
	static auto Register(World& world, SystemFlags systemFlags = SystemFlags::None);
	auto GetArchetype(Entity entity) const;
	const std::vector<Entity>& GetEntities();
	Query<Reject<Prefab>, Components...>* systemQuery{};
};

class World;

class SystemManager
{
	using SystemId = intptr_t;

	template <typename T>
	static constexpr SystemId GetSystemId()
	{
		return reinterpret_cast<SystemId>(typeid(T).name());
	}

public:
	template <typename T>
	auto RegisterSystem(World* world, SystemFlags systemFlags = SystemFlags::None) -> std::enable_if_t<std::is_base_of_v<SystemBase, T>, std::shared_ptr<T>>
	{
		SystemId systemId = GetSystemId<T>();

		ASSERT(!systems.contains(systemId) && "System already registerd.");

		auto system = std::make_shared<T>();
		system->world = world;
		system->flags = systemFlags;
		systems.insert({ systemId, system });
		return system;
	}

	template <typename T, typename Config>
	std::shared_ptr<T> RegisterSystemConfigured(World* world, const Config& config)
	{
		SystemId systemId = GetSystemId<T>();

		ASSERT(!systems.contains(systemId) && "System already registerd.");

		auto system = std::make_shared<T>(config);
		system->world = world;

		systems.insert({ systemId, system });
		return system;
	}

	template <typename T>
	void SetSignature(Signature signature)
	{
		SystemId systemId = GetSystemId<T>();

		ASSERT(systems.contains(systemId) && "System has not been registered.");

		signatures.insert({ systemId, signature });
	}

	template <typename T>
	std::shared_ptr<T> GetSystem()
	{
		SystemId systemId = GetSystemId<T>();

		ASSERT(systems.contains(systemId) && "System has not been registered.");

		return std::reinterpret_pointer_cast<T, SystemBase>(systems[systemId]);
	}

	template <typename T>
	Signature GetSystemSignature() const
	{
		SystemId systemId = GetSystemId<T>();

		ASSERT(signatures.contains(systemId) && "System has not been registered.");

		return signatures[systemId];
	}

private:
	std::unordered_map<SystemId, Signature> signatures{};
	std::unordered_map<SystemId, std::shared_ptr<SystemBase>> systems{};
};

class World
{
public:
	World()
		: entityManager(*this)
		, queryManager(*this)
	{
		RegisterComponent<Prefab>();
	}

	Entity CreateEntity()
	{
		return entityManager.CreateEntity();
	}

	template <int N>
	std::array<Entity, N> CreateEntities()
	{
		return entityManager.CreateEntities<N>();
	}

	std::vector<Entity> CreateEntities(size_t entityCount)
	{
		std::vector<Entity> result;
		for (size_t i = 0; i < entityCount; ++i)
			result.emplace_back(entityManager.CreateEntity());
		return result;
	}

	Entity CloneEntity(Entity entity)
	{
		if (!entity)
			return 0;

		Entity newEntity = CreateEntity();

		Signature signature = entityManager.GetSignature(entity);
		Signature newSignature{};
		ecs::Log("Clone Entity {} -> {}", entity, newEntity, signature);
		ecs::Log("    Require: {}", componentManager.BuildSignatureLayerString(signature.require));
		ecs::Log("    Reject: {}", componentManager.BuildSignatureLayerString(signature.reject));
		int nextTypeIndex = signature.require.lowest();
		while (nextTypeIndex >= 0)
		{
			ComponentType nextType = static_cast<ComponentType>(nextTypeIndex);

			signature.require.set(nextTypeIndex, false);
			nextTypeIndex = signature.require.lowest();

			if (nextType == GetComponentType<Prefab>())
				continue;

			const char* nextTypeName = componentManager.GetComponentTypeName(nextType);
			ecs::Log("    Add Component<{}>", nextTypeName);

			if (auto [ptr, size] = componentManager.TryGetComponent(entity, nextType); ptr)
				AddComponentUntypedNoNotify(newEntity, newSignature, nextType, ptr, size);
		}
		queryManager.OnEntitySignatureChanged(newEntity, newSignature, {});

		return newEntity;
	}

	void DestroyEntity(Entity entity)
	{
		ecs::Log("[World] DestroyEntity {}", entity);
		queryManager.OnEntitySignatureChanged(entity, Signature{}, entityManager.GetSignature(entity));
		entityManager.DestroyEntity(entity);
		componentManager.OnEntityDestroyed(entity);
	}

	template <typename T>
	void RegisterComponent()
	{
		componentManager.RegisterComponent<T>();
	}

	template <typename... Components>
	void RegisterComponents()
	{
		RegisterComponentsHelper<Components...>();
	}

	template <typename T>
	T& AddComponent(Entity entity, const T& component)
	{
		Signature signature = entityManager.GetSignature(entity);
		Signature oldSignature = signature;
		T& result = AddComponentNoNotify(entity, signature, component);

		queryManager.OnEntitySignatureChanged(entity, signature, oldSignature);

		return result;
	}

	template <typename T>
	auto AddTag(Entity entity) -> std::enable_if_t<std::is_empty_v<T>, void>
	{
		if (!HasComponent<T>(entity))
			AddComponent<T>(entity, {});
	}

	void* AddComponentUntyped(Entity entity, ComponentType componentType, const void* source, size_t size)
	{
		Signature signature = entityManager.GetSignature(entity);
		Signature oldSignature = signature;
		void* result = AddComponentUntypedNoNotify(entity, signature, componentType, source, size);

		queryManager.OnEntitySignatureChanged(entity, signature, oldSignature);

		return result;
	}

	template <typename... Components>
	std::tuple<Components&...> AddComponents(Entity entity, Components... components)
	{
		Signature signature = entityManager.GetSignature(entity);
		Signature oldSignature = signature;

		auto result = AddComponentsHelper(entity, signature, components...);

		queryManager.OnEntitySignatureChanged(entity, signature, oldSignature);

		return result;
	}

	template <typename T>
	void RemoveComponent(Entity entity)
	{
		componentManager.RemoveComponent<T>(entity);

		Signature signature = entityManager.GetSignature(entity);
		Signature oldSignature = signature;
		signature.require.set(componentManager.GetComponentType<T>(), false);
		entityManager.SetSignature(entity, signature);

		queryManager.OnEntitySignatureChanged(entity, signature, oldSignature);
	}

	template <typename T>
	bool HasComponent(Entity entity)
	{
		return componentManager.HasComponent<T>(entity);
	}

	template <typename T>
	T& GetComponent(Entity entity)
	{
		return componentManager.GetComponent<T>(entity);
	}

	template <typename T>
	std::optional<std::reference_wrapper<T>> GetOptionalComponent(Entity entity)
	{
		if (HasComponent<T>(entity))
			return componentManager.GetComponent<T>(entity);
		return {};
	}

	template <typename T>
	T& GetOrAddComponent(Entity entity, const T& component)
	{
		if (HasComponent<T>(entity))
			return GetComponent<T>(entity);
		return AddComponent(entity, component);
	}

	template <typename... Components>
	auto GetComponents(Entity entity)
	{
		return GetComponentsHelper<Components...>(entity);
	}

	template <typename T>
	ComponentType GetComponentType() const
	{
		return componentManager.GetComponentType<T>();
	}

	const char* GetComponentTypeName(ComponentType componentType) const
	{
		return componentManager.GetComponentTypeName(componentType);
	}

	std::string BuildSignatureLayerString(Signature::Layer layer) const
	{
		return componentManager.BuildSignatureLayerString(layer);
	}

	template <typename T, typename... Components>
	std::shared_ptr<T> RegisterSystem(SystemFlags systemFlags = SystemFlags::None)
	{
		auto system = systemManager.RegisterSystem<T>(this, systemFlags);
		Signature signature = BuildSignature<Reject<Prefab>, Components...>();
		SetSystemSignature<T>(signature);
		system->GetSystemQuery();
		system->OnRegistered();
		return system;
	}

	template <typename T, typename... Components>
	std::shared_ptr<T> RegisterSystemConfigured(const typename T::Config& config)
	{
		auto system = systemManager.RegisterSystemConfigured<T, typename T::Config>(this, config);
		Signature signature = BuildSignature<Reject<Prefab>, Components...>();
		SetSystemSignature<T>(signature);
		return system;
	}

	template <typename T>
	void SetSystemSignature(Signature signature)
	{
		systemManager.SetSignature<T>(signature);
	}

	template <typename T>
	std::shared_ptr<T> GetSystem()
	{
		return systemManager.GetSystem<T>();
	}

	template <typename T>
	Signature GetSystemSignature() const
	{
		return systemManager.GetSystemSignature<T>();
	}

	template <typename... Components>
	Query<Components...>* CreateQuery(QueryCallbacks callbacks = {}, QueryBase::SortPredicate sortPredicate = DefaultQuerySort)
	{
		auto query = queryManager.CreateQuery<Components...>(callbacks, sortPredicate);
		query->InitializeEntityList(entityManager);
		return query;
	}

	template <typename... Components>
	Signature BuildSignature() const
	{
		return componentManager.BuildSignature<Components...>();
	}

	Entity GetEntityCount() const { return entityManager.GetEntityCount(); }

private:
	template <typename Head, typename... Tail>
	void RegisterComponentsHelper()
	{
		RegisterComponent<Head>();
		if constexpr (sizeof...(Tail) > 0)
			RegisterComponentsHelper<Tail...>();
	}

	template <typename T>
	auto AddComponentHelper(Entity entity, const T& component)
	{
		return std::tuple<T&>(AddComponent(entity, component));
	}

	template <typename Head, typename... Tail>
	std::tuple<Head&, Tail&...> AddComponentsHelper(Entity entity, Signature& signature, const Head& head, const Tail&... tail)
	{
		if constexpr (sizeof...(tail) > 0)
		{
			auto first = std::tuple<Head&>(AddComponentNoNotify(entity, signature, head));
			return std::tuple_cat(first, AddComponentsHelper(entity, signature, tail...));
		}
		else
			return std::tuple<Head&>(AddComponentNoNotify(entity, signature, head));
	}

	template <typename T>
	auto GetFirstHelper(Entity entity)
	{
		if constexpr (is_reject_component_v<T>)
			return std::tuple<>();
		else
			return std::tuple<T&>(GetComponent<T>(entity));
	}

	template <typename Head, typename... Tail>
	auto GetComponentsHelper(Entity entity)
	{
		if constexpr (sizeof...(Tail) > 0)
			return std::tuple_cat(GetFirstHelper<Head>(entity), GetComponentsHelper<Tail...>(entity));
		else
			return GetFirstHelper<Head>(entity);
	}

	template <typename T>
	T& AddComponentNoNotify(Entity entity, Signature& signature, const T& component)
	{
		ecs::Log("AddComponent<{}> to Entity {}", typeid(T).name() + 7, entity);

		T& result = componentManager.AddComponent<T>(entity, component);

		signature.require.set(componentManager.GetComponentType<T>(), true);
		entityManager.SetSignature(entity, signature);

		return result;
	}

	void* AddComponentUntypedNoNotify(Entity entity, Signature& signature, ComponentType componentType, const void* source, size_t size)
	{
		ecs::Log("AddComponent<{}> to Entity {}", GetComponentTypeName(componentType), entity);

		void* result = componentManager.AddComponentUntyped(entity, componentType, source, size);

		signature.require.set(componentType, true);
		entityManager.SetSignature(entity, signature);

		return result;
	}

private:
	EntityManager entityManager;
	ComponentManager componentManager;
	SystemManager systemManager;
	QueryManager queryManager;
};

inline void LogSignature(const World& world, const char* label, Signature signature)
{
	ecs::Log("{}: Signature {}", label, signature);
	ecs::Log("    Require: {}", world.BuildSignatureLayerString(signature.require));
	ecs::Log("    Reject:  {}", world.BuildSignatureLayerString(signature.reject));
}

inline void LogCompareSignatures(const World& world, const char* label1, Signature signature1, const char* label2, Signature signature2)
{
	ecs::Log("Compare 1: {} {} -> 2: {} {}", label1, signature1, label2, signature2);
	ecs::Log("    Require 1: {}", world.BuildSignatureLayerString(signature1.require));
	ecs::Log("    Require 2: {}", world.BuildSignatureLayerString(signature2.require));
	ecs::Log("    Reject  1: {}", world.BuildSignatureLayerString(signature1.reject));
	ecs::Log("    Reject  2: {}", world.BuildSignatureLayerString(signature2.reject));
}

template <typename T, typename ... Components>
auto System<T, Components...>::Register(World& world, SystemFlags systemFlags)
{
	return world.RegisterSystem<T, Components...>(systemFlags);
}

template <typename T, typename ... Components>
auto System<T, Components...>::GetArchetype(Entity entity) const
{
	return GetWorld().template GetComponents<Components...>(entity);
}

template <typename T, typename ... Components>
const std::vector<Entity>& System<T, Components...>::GetEntities()
{
	return GetSystemQuery()->GetEntities();
}

template <typename T, typename ... Components>
auto System<T, Components...>::GetSystemQuery()
{
	if (!systemQuery)
		systemQuery = GetWorld().template CreateQuery<Reject<Prefab>, Components...>(
			QueryCallbacks{
				[this](Entity e) { static_cast<T*>(this)->OnEntityAdded(e); },
				[this](Entity e) { static_cast<T*>(this)->OnEntityRemoved(e); }
			});
	return systemQuery;
}

template <class F, class... Args>
void for_each_argument(F f, Args&&... args) {
	(void)std::initializer_list<int>{(f(std::forward<Args>(args)), 0)...};
}

template <typename TVec, typename TFieldTuple, std::size_t... TIdxs>
void insert_tuple_vector(TVec& vec, ptrdiff_t index, TFieldTuple ft, std::index_sequence<TIdxs...>)
{
	for_each_argument([&](auto idx)
		{
			auto insertAt = std::get<idx>(vec).begin() + index;
			std::get<idx>(vec).insert(insertAt, std::get<idx>(ft));
		}, std::integral_constant<std::size_t, TIdxs>{}...);
}

template <typename TVec, std::size_t... TIdxs>
void erase_tuple_vector(TVec& vec, ptrdiff_t index, std::index_sequence<TIdxs...>)
{
	for_each_argument([&](auto idx)
		{
			auto eraseAt = std::get<idx>(vec).begin() + index;
			std::get<idx>(vec).erase(eraseAt);
		}, std::integral_constant<std::size_t, TIdxs>{}...);
}

template <typename... Components>
void Query<Components...>::InsertLists(ptrdiff_t index, Entity entity)
{
	auto comps = GetArchetype(entity);
	
	insert_tuple_vector(componentLists,
		index,
		comps,
		std::make_index_sequence<std::tuple_size_v<decltype(comps)>>());
}

template <typename... Components>
void Query<Components...>::RemoveLists(ptrdiff_t index)
{
	QueryBase::RemoveLists(index);

	erase_tuple_vector(componentLists, index, std::make_index_sequence<std::tuple_size_v<decltype(componentLists)>>());
}

template <class... Components>
Query<Components...>* QueryManager::CreateQuery(QueryCallbacks callbacks, QueryBase::SortPredicate sortPredicate)
{
	Signature signature = world.BuildSignature<Components...>();

	signatures.insert(signature);

	QueryId queryId = nextQueryId++;
	queriesBySignature[signature].emplace_back(queryId);

	LogSignature(world, "Create Query", signature);
	queries[queryId] = std::make_unique<Query<Components...>>(queryId, &world, signature, callbacks, std::move(sortPredicate));
	QueryBase* baseQuery = queries[queryId].get();
	return static_cast<Query<Components...>*>(baseQuery);
}


inline QueryBase* QueryManager::GetQueryUntypedById(QueryId queryId) const
{
	if (queryId > 0 && queries.contains(queryId))
	{
		return queries.at(queryId).get();
	}
	return nullptr;
}

template <class ... Components>
Query<Components...>* QueryManager::GetQueryById(QueryId queryId)
{
	return static_cast<Query<Components...>*>(GetQueryUntypedById(queryId));
}

// ReSharper disable once CppMemberFunctionMayBeConst
inline void QueryManager::OnEntitySignatureChanged(Entity entity, Signature newSignature, Signature oldSignature)
{
	if (newSignature == oldSignature)
		return;

	ecs::Log("[QueryManager] OnEntitySignatureChanged {}", entity);

	for (const Signature& signature : signatures)
	{
		if (signature.Matches(newSignature) && !signature.Matches(oldSignature))
		{
			for (QueryId queryId : queriesBySignature[signature])
			{
				// Entity did not match query but now does, add it to the query entity list
				QueryBase* query = GetQueryUntypedById(queryId);

				ecs::Log("Query {} match Entity {}", queryId, entity);
				LogCompareSignatures(world, "Match Query", signature, "Entity", newSignature);

#ifdef _DEBUG
				auto search = std::ranges::find(query->GetEntities(), entity);
				ASSERT(search == query->GetEntities().end() && "Entity already exists in query.");
#endif

				query->AddEntity(entity);
			}
		}
		else if (!signature.Matches(newSignature) && signature.Matches(oldSignature))
		{
			for (QueryId queryId : queriesBySignature[signature])
			{
				// Entity no longer matches query but used to so remove it from the query entity list
				QueryBase* query = GetQueryUntypedById(queryId);

				ecs::Log("Query {} unmatch Entity {}", queryId, entity);
				LogCompareSignatures(world, "Unmatch Query", signature, "Entity", newSignature);

#ifdef _DEBUG
				auto search = std::ranges::find(query->GetEntities(), entity);
				ASSERT(search != query->GetEntities().end() && "Entity did not exist in query.");
#endif

				query->RemoveEntity(entity);
			}

		}
	}
}
