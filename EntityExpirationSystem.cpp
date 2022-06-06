#include "EntityExpirationSystem.h"

#include "components.h"

void EntityExpirationSystem::Update(const GameTime& time) const
{
	std::queue<Entity> removeQueue{};

	for (Entity entity : entities)
	{
		auto& [secRemaining] = GetWorld().GetComponent<Expiration>(entity);

		secRemaining -= time.dt();

		if (secRemaining <= 0)
			removeQueue.push(entity);
	}

	while (!removeQueue.empty())
	{
		GetWorld().DestroyEntity(removeQueue.front());
		removeQueue.pop();
	}
}
