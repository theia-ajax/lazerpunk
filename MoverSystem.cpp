#include "MoverSystem.h"
#include "components.h"

void MoverSystem::Update(const GameTime& time) const
{
	for (Entity entity : entities)
	{
		auto& transform = GetWorld().GetComponent<Transform>(entity);
		auto& [velocity] = GetWorld().GetComponent<Velocity>(entity);
		transform.position = transform.position + velocity * time.dt();
	}
}
