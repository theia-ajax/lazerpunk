#include "EnemyFollowTargetSystem.h"

#include "components.h"

void EnemyFollowTargetSystem::Update(const GameTime& time) const
{
	if (!targetEntity)
		return;

	const auto& targetTransform = GetWorld().GetComponent<Transform>(targetEntity);

	for (Entity entity : entities)
	{
		auto [transform, velocity, _] = GetArchetype(entity);

		Vec2 delta = targetTransform.position - transform.position;
		Vec2 dir = vec2::Normalize(delta);
		Vec2 vel = velocity.velocity;
		vel = vec2::Damp(vel, dir * 10, 0.4f, time.dt());
		velocity.velocity = vel;
	}
}
