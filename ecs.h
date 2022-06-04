#pragma once

#include "types.h"
#include <bitset>
#include <queue>
#include <unordered_map>
#include <set>
#include <memory>
#include <vcruntime_typeinfo.h>

class World;
using Entity = uint32_t;
constexpr Entity kMaxEntities = 4096;

using ComponentType = uint8_t;
constexpr ComponentType kMaxComponents = 64;

using Signature = std::bitset<kMaxComponents>;



class EntityManager
{
public:
	EntityManager()
	{
		for (Entity entity = 0; entity < kMaxEntities; ++entity)
			availableEntities.push(entity);
	}

	Entity CreateEntity()
	{
		ASSERT(!availableEntities.empty() && "Max entities reached.");

		Entity id = availableEntities.front();
		availableEntities.pop();

		return id;
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

private:
	std::queue<Entity> availableEntities{};
	std::array<Signature, kMaxEntities> signatures{};
};

class IComponentArray
{
public:
	virtual ~IComponentArray() = default;
	virtual void OnEntityDestroyed(Entity entity) = 0;
};

template <typename T>
class ComponentArray : public IComponentArray
{
public:
	T& Insert(Entity entity, T component)
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
	T& AddComponent(Entity entity, T component)
	{
		return GetComponentArray<T>()->Insert(entity, component);
	}

	template <typename T>
	void RemoveComponent(Entity entity)
	{
		GetComponentArray<T>()->Remove(entity);
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

struct System
{
	World& GetWorld() const { return *world; }
	std::set<Entity> entities;
	friend class SystemManager;
private:
	void SetWorld(World* world) { this->world = world; }
	World* world{};
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
	template <typename T>
	std::shared_ptr<T> RegisterSystem(World* world)
	{
		SystemId systemId = GetSystemId<T>();

		ASSERT(!systems.contains(systemId) && "System already registerd.");

		auto system = std::make_shared<T>();
		system->SetWorld(world);

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
				system->entities.insert(entity);
			}
			else
			{
				system->entities.erase(entity);
			}
		}
	}

private:
	std::unordered_map<SystemId, Signature> signatures{};
	std::unordered_map<SystemId, std::shared_ptr<System>> systems{};
};

class World
{
public:
	Entity CreateEntity()
	{
		return entityManager.CreateEntity();
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

	template <typename T>
	T& AddComponent(Entity entity, T component)
	{
		T& result = componentManager.AddComponent<T>(entity, component);

		Signature signature = entityManager.GetSignature(entity);
		signature.set(componentManager.GetComponentType<T>(), true);
		entityManager.SetSignature(entity, signature);

		systemManager.OnEntitySignatureChanged(entity, signature);

		return result;
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
	T& GetComponent(Entity entity)
	{
		return componentManager.GetComponent<T>(entity);
	}

	template <typename T>
	ComponentType GetComponentType() const
	{
		return componentManager.GetComponentType<T>();
	}


	template <typename T, typename... Components>
	std::shared_ptr<T> RegisterSystem()
	{
		auto system = systemManager.RegisterSystem<T>(this);
		Signature signature = BuildSignature<Components...>();
		SetSystemSignature<T>(signature);
		return system;
	}

	template <typename T>
	void SetSystemSignature(Signature signature)
	{
		systemManager.SetSignature<T>(signature);
	}

private:
	template <typename Head, typename... Tail>
	Signature BuildSignature() const
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
	EntityManager entityManager;
	ComponentManager componentManager;
	SystemManager systemManager;
};