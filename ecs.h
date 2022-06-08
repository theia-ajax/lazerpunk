#pragma once

// Adapted from the example provided on this blog post
// https://austinmorlan.com/posts/entity_component_system


#include <bitset>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <unordered_map>
#include "types.h"

class World;
using Entity = uint32_t;
constexpr Entity kMaxEntities = 4096;
constexpr Entity kInvalidEntity = 0;

using ComponentType = uint8_t;
constexpr ComponentType kMaxComponents = 64;

// Signatures represent which 
using Signature = std::bitset<kMaxComponents>;

// Tracks available entity indices and signatures of active entities
class EntityManager
{
public:
	EntityManager()
	{
		for (Entity entity = 1; entity <= kMaxEntities; ++entity)
			availableEntities.push(entity);
	}

	Entity CreateEntity()
	{
		ASSERT(!availableEntities.empty() && "Max entities reached.");

		Entity id = availableEntities.front();
		availableEntities.pop();

		return id;
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
		return kMaxEntities - static_cast<Entity>(availableEntities.size());
	}

private:
	std::queue<Entity> availableEntities{};
	std::array<Signature, kMaxEntities + 1> signatures{};
};

class IComponentArray
{
public:
	virtual ~IComponentArray() = default;
	virtual void OnEntityDestroyed(Entity entity) = 0;
};

template <typename T>
class ComponentArray final : public IComponentArray
{
public:
	T& Insert(Entity entity, const T& component)
	{
		ASSERT(!entityToIndexMap.contains(entity) && "Component already added to entity.");

		size_t newIndex = size++;
		entityToIndexMap[entity] = newIndex;
		indexToEntityMap[newIndex] = entity;
		componentArray[newIndex] = component;

		return componentArray[newIndex];
	}

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

	T* begin() { return &componentArray[0]; }
	T* end() { return &componentArray[size]; }

	void OnEntityDestroyed(Entity entity) override
	{
		if (entityToIndexMap.contains(entity))
		{
			Remove(entity);
		}
	}

private:
	std::array<T, kMaxEntities> componentArray{};
	std::unordered_map<Entity, size_t> entityToIndexMap{};
	std::unordered_map<size_t, Entity> indexToEntityMap{};
	size_t size{};
};

class ComponentManager
{
	using ComponentId = StrId;

	template <typename T>
	static constexpr ComponentId GetComponentId()
	{
		return ComponentId(typeid(T).name());
	}

public:
	template <typename T>
	void RegisterComponent()
	{
		ComponentId componentId = GetComponentId<T>();
		ASSERT(!componentTypes.contains(componentId) && "Component already registered.");
		componentTypes.insert({ componentId, nextComponentType });
		componentArrays.insert({ componentId, std::make_shared<ComponentArray<T>>() });
		++nextComponentType;
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

	void OnEntityDestroyed(Entity entity)
	{
		for (const auto& [_, component] : componentArrays)
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
		ComponentType componentType = GetComponentType<Head>();
		signature.set(componentType, true);
		if constexpr (sizeof...(Tail) > 0)
		{
			signature |= BuildSignature<Tail...>();
		}
		return signature;
	}

private:
	std::unordered_map<ComponentId, ComponentType> componentTypes{};
	std::unordered_map<ComponentId, std::shared_ptr<IComponentArray>> componentArrays;
	ComponentType nextComponentType{};

	template <typename T>
	std::shared_ptr<ComponentArray<T>> GetComponentArray()
	{
		ComponentId componentId = GetComponentId<T>();
		ASSERT(componentTypes.contains(componentId) && "Component not registerd.");
		return std::static_pointer_cast<ComponentArray<T>>(componentArrays[componentId]);
	}
};

enum class SystemFlags
{
	None = 0,
	Monitor = 1 << 0,
};

struct SystemBase  // NOLINT(cppcoreguidelines-special-member-functions)
{
	virtual ~SystemBase();

	World& GetWorld() const;
	std::set<Entity> entities{};

	friend class SystemManager;

	virtual void OnEntityAdded(Entity entity);
	virtual void OnEntityRemoved(Entity entity);

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


template <typename T, typename... Components>
struct System : SystemBase
{
	static auto Register(World& world, SystemFlags systemFlags = SystemFlags::None);
	std::tuple<Components&...> GetArchetype(Entity entity) const;
};

class World;

class SystemManager
{
	using SystemId = StrId;

	template <typename T>
	static constexpr SystemId GetSystemId()
	{
		return SystemId(typeid(T).name());
	}

public:
	template <typename T, std::enable_if_t<std::is_base_of_v<SystemBase, T>, bool> = true>
	std::shared_ptr<T> RegisterSystem(World* world, SystemFlags systemFlags = SystemFlags::None)
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
		for (const auto& [_, system] : systems)
		{
			system->entities.erase(entity);
		}
	}

	void OnEntitySignatureChanged(Entity entity, Signature entitySignature)
	{
		for (const auto& [systemId, system] : systems)
		{
			if (const auto& systemSignature = signatures[systemId]; (entitySignature & systemSignature) == systemSignature)
			{
				if (system->Flags() != SystemFlags::None)
				{
					int p = 0;
				}
				if (flags::Test(system->Flags(), SystemFlags::Monitor) && !system->entities.contains(entity))
					system->OnEntityAdded(entity);

				system->entities.insert(entity);

			}
			else
			{
				if (flags::Test(system->Flags(), SystemFlags::Monitor) && system->entities.contains(entity))
					system->OnEntityRemoved(entity);

				system->entities.erase(entity);
			}
		}
	}

private:
	std::unordered_map<SystemId, Signature> signatures{};
	std::unordered_map<SystemId, std::shared_ptr<SystemBase>> systems{};
};

class World
{
public:
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

	void DestroyEntity(Entity entity)
	{
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
		signature.set(componentManager.GetComponentType<T>(), true);
		entityManager.SetSignature(entity, signature);

		systemManager.OnEntitySignatureChanged(entity, signature);

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
		signature.set(componentManager.GetComponentType<T>(), false);
		entityManager.SetSignature(entity, signature);

		systemManager.OnEntitySignatureChanged(entity, signature);
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
		else
			return AddComponent(entity, component);
	}

	template <typename... Components>
	std::tuple<Components&...> GetComponents(Entity entity)
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
		Signature signature = BuildSignature<Components...>();
		SetSystemSignature<T>(signature);
		return system;
	}

	template <typename T, typename... Components>
	std::shared_ptr<T> RegisterSystemConfigured(const typename T::Config& config)
	{
		auto system = systemManager.RegisterSystemConfigured<T, typename T::Config>(this, config);
		Signature signature = BuildSignature<Components...>();
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

	template <typename Head, typename... Tail>
	std::tuple<Head&, Tail&...> AddComponentsHelper(Entity entity, Head head, Tail... tail)
	{
		std::tuple<Head&> first = AddComponent(entity, head);
		if constexpr (sizeof...(tail) > 0)
		{
			std::tuple<Tail&...> rest = AddComponentsHelper(entity, tail...);
			return std::tuple_cat(first, rest);
		}
		else
		{
			return first;
		}
	}

	template <typename Head, typename... Tail>
	std::tuple<Head&, Tail&...> GetComponentsHelper(Entity entity)
	{
		std::tuple<Head&> first = GetComponent<Head>(entity);
		if constexpr (sizeof...(Tail) > 0)
		{
			std::tuple<Tail&...> rest = GetComponentsHelper<Tail...>(entity);
			return std::tuple_cat(first, rest);
		}
		else
		{
			return first;
		}
	}

private:
	EntityManager entityManager;
	ComponentManager componentManager;
	SystemManager systemManager;
};

template <typename T, typename ... Components>
auto System<T, Components...>::Register(World& world, SystemFlags systemFlags)
{
	return world.RegisterSystem<T, Components...>(systemFlags);
}

template <typename T, typename ... Components>
std::tuple<Components&...> System<T, Components...>::GetArchetype(Entity entity) const
{
	return GetWorld().template GetComponents<Components...>(entity);
}
