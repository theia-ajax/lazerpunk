#pragma once

// Adapted from the example provided on this blog post
// https://austinmorlan.com/posts/entity_component_system


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

class World;
using Entity = int32_t;
constexpr Entity kMaxEntities = 4096;
constexpr Entity kInvalidEntity = 0;

using ComponentType = uint8_t;
constexpr ComponentType kMaxComponents = 64;

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

struct Prefab {};

// Signatures represent which 
struct Signature
{
	bitfield<kMaxComponents> signature;
	bitfield<kMaxComponents> ignored;

	void reset() { signature.reset(); ignored.reset(); }
	bool Matches(const Signature& other) const
	{
		return (signature & other.signature) == signature &&
			(ignored & other.signature).empty();
	}

	Signature& operator|=(const Signature& other)
	{
		signature |= other.signature;
		ignored |= other.ignored;
		return *this;
	}
};

inline bool operator==(const Signature& a, const Signature& b) { return a.Matches(b); }
inline bool operator<(const Signature& a, const Signature& b)
{
	return (a.signature < b.signature) || ((a.signature == b.signature) && a.ignored < b.ignored);
}

template<>
struct std::hash<Signature>
{
	size_t operator()(const Signature& signature) const noexcept
	{
		size_t res = 17;
		res = res * 31 + signature.signature.hash();
		res = res * 31 + signature.ignored.hash();
		return res;
	}
};

// Tracks available entity indices and signatures of active entities
class EntityManager
{
public:
	EntityManager()
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
				entities.emplace_back(entity);
		}
	}

private:
	std::stack<Entity> availableEntities{};
	std::set<Entity> activeEntities;
	std::array<Signature, kMaxEntities + 1> signatures{};
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

public:
	template <typename T>
	auto RegisterComponent() -> std::enable_if_t<std::is_trivially_copyable_v<T>, ComponentId>
	{
		ComponentId componentId = GetComponentId<T>();
		ASSERT(!componentTypes.contains(componentId) && "Component already registered.");
		componentTypes.insert({ componentId, nextComponentType });
		componentIds.insert({ nextComponentType, componentId });
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

private:
	template <typename Head, typename... Tail>
	Signature BuildSignatureHelper() const
	{
		Signature signature;
		if constexpr (is_reject_component_v<Head>)
		{
			ComponentType ignoredType = GetComponentType<typename Head::Component>();
			signature.ignored.set(ignoredType, true);
		}
		else
		{
			ComponentType componentType = GetComponentType<Head>();
			signature.signature.set(componentType, true);
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

class QueryBase  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	QueryBase() = delete;
	QueryBase(const QueryBase& other) = delete;
	QueryBase(QueryBase&& other) = delete;
	QueryBase operator=(const QueryBase& other) = delete;
	QueryBase operator=(QueryBase&& other) = delete;

	explicit QueryBase(World* world, Signature signature, std::function<void(Entity)> onEntityMatch = {}, std::function<void(Entity)> onEntityUnmatch = {})
		: signature(signature), onEntityMatchQuery(std::move(onEntityMatch)), onEntityUnmatchQuery(std::move(onEntityUnmatch)), world(world) {}

	World& GetWorld() const { return *world; }
	std::vector<Entity>& GetEntities() { return entities; }
	Signature GetSignature() const { return signature; }
	void OnEntityMatch(Entity entity) const { if (onEntityMatchQuery) onEntityMatchQuery(entity); }
	void OnEntityUnmatch(Entity entity) const { if (onEntityUnmatchQuery) onEntityUnmatchQuery(entity); }
	void SetOnEntityMatch(std::function<void(Entity)> action) { onEntityMatchQuery = std::move(action); }
	void SetOnEntityUnmatch(std::function<void(Entity)> action) { onEntityUnmatchQuery = std::move(action); }
protected:
	std::vector<Entity> entities;
	Signature signature;
	std::function<void(Entity)> onEntityMatchQuery = {};
	std::function<void(Entity)> onEntityUnmatchQuery = {};

private:
	World* world;
};

template <typename... Components>
class Query final : public QueryBase
{
public:
	Query() = delete;
	Query(const Query& other) = delete;
	Query(Query&& other) = delete;
	Query operator=(const Query& other) = delete;
	Query operator=(Query&& other) = delete;

	explicit Query(World* world, Signature signature, std::function<void(Entity)> onEntityMatch = {}, std::function<void(Entity)> onEntityUnmatch = {})
		: QueryBase(world, signature, onEntityMatch, onEntityUnmatch) { }

	auto GetArchetype(Entity entity) const
	{
		return GetWorld().template GetComponents<Components...>(entity);
	}
};

class QueryManager
{
public:
	explicit QueryManager(World& world) : world(world) {}

	template <class ... Components>
	auto GetOrCreateQuery(std::function<void(Entity)> onMatch = {}, std::function<void(Entity)> onUnmatch = {}) -> std::pair<Query<Components...>*, bool>;
	void OnEntitySignatureChanged(Entity entity, Signature newSignature, Signature oldSignature);
private:
	World& world;

	std::set<Signature> signatures;
	std::unordered_map<Signature, std::unique_ptr<QueryBase>> queries;
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
	std::set<Entity> entities{};

	friend class SystemManager;

	virtual void OnEntityAdded(Entity entity);
	virtual void OnEntityRemoved(Entity entity);
	virtual void OnEntityDestroyed(Entity entity);

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
inline void SystemBase::OnEntityDestroyed(Entity destroyedEntity) {}


template <typename T, typename... Components>
struct System : SystemBase
{
	auto GetSystemQuery();
	static auto Register(World& world, SystemFlags systemFlags = SystemFlags::None);
	auto GetArchetype(Entity entity) const;
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

	void OnEntityDestroyed(Entity entity)
	{
		for (const auto& system : systems | std::views::values)
		{
			if (flags::Test(system->Flags(), SystemFlags::MonitorGlobalEntityDestroy))
				system->OnEntityDestroyed(entity);
			RemoveEntityFromSystem(entity, system);
		}
	}

	void OnEntitySignatureChanged(Entity entity, Signature entitySignature)
	{
		for (const auto& [systemId, system] : systems)
		{
			if (const auto& systemSignature = signatures[systemId]; systemSignature.Matches(entitySignature))
			{
				AddEntityToSystem(entity, system);
			}
			else
			{
				RemoveEntityFromSystem(entity, system);
			}
		}
	}

private:
	static void AddEntityToSystem(Entity entity, const std::shared_ptr<SystemBase>& system)
	{
		if (flags::Test(system->Flags(), SystemFlags::Monitor) && !system->entities.contains(entity))
			system->OnEntityAdded(entity);

		system->entities.insert(entity);
	}

	static void RemoveEntityFromSystem(Entity entity, const std::shared_ptr<SystemBase>& system)
	{
		if (flags::Test(system->Flags(), SystemFlags::Monitor) && system->entities.contains(entity))
			system->OnEntityRemoved(entity);

		system->entities.erase(entity);
	}

private:
	std::unordered_map<SystemId, Signature> signatures{};
	std::unordered_map<SystemId, std::shared_ptr<SystemBase>> systems{};
};

class World
{
public:
	World()
		:  queryManager(*this)
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

		auto [signature, ignored] = entityManager.GetSignature(entity);
		int nextTypeIndex = signature.lowest();
		while (nextTypeIndex >= 0)
		{
			ComponentType nextType = static_cast<ComponentType>(nextTypeIndex);

			signature.set(nextTypeIndex, false);
			nextTypeIndex = signature.lowest();

			if (nextType == GetComponentType<Prefab>())
				continue;

			if (auto [ptr, size] = componentManager.TryGetComponent(entity, nextType); ptr)
				AddComponentUntyped(newEntity, nextType, ptr, size);
		}

		return newEntity;
	}

	void DestroyEntity(Entity entity)
	{
		queryManager.OnEntitySignatureChanged(entity, Signature{}, entityManager.GetSignature(entity));
		entityManager.DestroyEntity(entity);
		componentManager.OnEntityDestroyed(entity);
		systemManager.OnEntityDestroyed(entity);
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
		T& result = componentManager.AddComponent<T>(entity, component);

		Signature signature = entityManager.GetSignature(entity);
		Signature oldSignature = signature;
		signature.signature.set(componentManager.GetComponentType<T>(), true);
		entityManager.SetSignature(entity, signature);

		systemManager.OnEntitySignatureChanged(entity, signature);
		queryManager.OnEntitySignatureChanged(entity, signature, oldSignature);

		return result;
	}

	void* AddComponentUntyped(Entity entity, ComponentType componentType, const void* source, size_t size)
	{
		void* result = componentManager.AddComponentUntyped(entity, componentType, source, size);
		Signature signature = entityManager.GetSignature(entity);
		Signature oldSignature = signature;
		signature.signature.set(componentType, true);
		entityManager.SetSignature(entity, signature);

		systemManager.OnEntitySignatureChanged(entity, signature);
		queryManager.OnEntitySignatureChanged(entity, signature, oldSignature);

		return result;
	}

	template <typename... Components>
	std::tuple<Components&...> AddComponents(Entity entity, Components... components)
	{
		return AddComponentsHelper(entity, components...);
	}

	template <typename T>
	void RemoveComponent(Entity entity)
	{
		componentManager.RemoveComponent<T>(entity);

		Signature signature = entityManager.GetSignature(entity);
		Signature oldSignature = signature;
		signature.signature.set(componentManager.GetComponentType<T>(), false);
		entityManager.SetSignature(entity, signature);

		systemManager.OnEntitySignatureChanged(entity, signature);
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

	template <typename T, typename... Components>
	std::shared_ptr<T> RegisterSystem(SystemFlags systemFlags = SystemFlags::None)
	{
		auto system = systemManager.RegisterSystem<T>(this, systemFlags);
		Signature signature = BuildSignature<Reject<Prefab>, Components...>();
		SetSystemSignature<T>(signature);
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
	Query<Components...>* CreateQuery(std::function<void(Entity)> onMatch = {},
	                                  std::function<void(Entity)> onUnmatch = {})
	{
		auto [query, isNew] = queryManager.GetOrCreateQuery<Components...>(onMatch, onUnmatch);
		if (isNew)
		{
			entityManager.GetEntitiesMatchingSignature(query->GetSignature(), query->GetEntities());
		}
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
	std::tuple<Head&, Tail&...> AddComponentsHelper(Entity entity, const Head& head, const Tail&... tail)
	{
		if constexpr (sizeof...(tail) > 0)
			return std::tuple_cat(std::tuple<Head&>(AddComponent(entity, head)), AddComponentsHelper(entity, tail...));
		else
			return std::tuple<Head&>(AddComponent(entity, head));
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

private:
	EntityManager entityManager;
	ComponentManager componentManager;
	SystemManager systemManager;
	QueryManager queryManager;
};


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
auto System<T, Components...>::GetSystemQuery()
{
	return GetWorld().template CreateQuery<Reject<Prefab>, Components...>(
		[this](Entity e) { static_cast<T*>(this)->OnEntityAdded(e); },
		[this](Entity e) { static_cast<T*>(this)->OnEntityRemoved(e); });
}

template <typename ... Components>
auto QueryManager::GetOrCreateQuery(std::function<void(Entity)> onMatch, std::function<void(Entity)> onUnmatch) -> std::pair<Query<Components...>*, bool>
{
	Signature signature = world.BuildSignature<Components...>();

	signatures.emplace(signature);
	bool isNewQuery = false;
	if (!queries.contains(signature))
	{
		queries[signature] = std::make_unique<Query<Components...>>(&world, signature, onMatch, onUnmatch);
		isNewQuery = true;
	}
	return std::make_pair(static_cast<Query<Components...>*>(queries[signature].get()), isNewQuery);
}

// ReSharper disable once CppMemberFunctionMayBeConst
inline void QueryManager::OnEntitySignatureChanged(Entity entity, Signature newSignature, Signature oldSignature)
{
	for (Signature signature : signatures)
	{
		if (signature.Matches(newSignature) && !signature.Matches(oldSignature))
		{
			// Entity did not match query but now does, add it to the query entity list
			QueryBase* query = queries[signature].get();
			query->GetEntities().emplace_back(entity);
			query->OnEntityMatch(entity);
		}
		else if (!signature.Matches(newSignature) && signature.Matches(oldSignature))
		{
			// Entity no longer matches query but used to so remove it from the query entity list
			QueryBase* query = queries[signature].get();
			std::erase_if(query->GetEntities(), [entity](Entity e) { return e == entity; });
			query->OnEntityUnmatch(entity);
		}
	}
}
