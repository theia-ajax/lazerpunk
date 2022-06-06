#include "EnemyFollowTargetSystem.h"

#include "components.h"

void EnemyFollowTargetSystem::Update(const GameTime& time) const
{
	if (!targetEntity)
		return;

	const auto& targetTransform = GetWorld().GetComponent<Transform>(targetEntity);

	std::vector<Entity> compareEntities{};
	compareEntities.reserve(entities.size());

	for (Entity entity : entities)
	{
		auto [transform, velocity] = GetWorld().GetComponents<Transform, Velocity>(entity);

		Vec2 delta = targetTransform.position - transform.position;
		Vec2 dir = vec2::Normalize(delta);
		Vec2 vel = vec2::Damp(velocity.velocity, dir * 10, 0.4f, time.dt());
		velocity.velocity = vel;

		compareEntities.emplace_back(entity);
	}

	for (size_t i = 0; i < compareEntities.size() - 1; ++i)
	{
		Entity entity0 = compareEntities[i];
		auto [tx0, vel0] = GetWorld().GetComponents<Transform, Velocity>(entity0);

		for (size_t j = i + 1; j < compareEntities.size(); ++j)
		{
			Entity entity1 = compareEntities[j];

			auto [tx1, vel1] = GetWorld().GetComponents<Transform, Velocity>(entity1);

			Vec2 delta = tx1.position - tx0.position;
			if (vec2::LengthSqr(delta) < 1.0f)
			{
				Vec2 dir = vec2::Normalize(delta);
				vel0.velocity = vel0.velocity - dir * 0.05f;
				vel1.velocity = vel1.velocity + dir * 0.05f;
			}
		}
	}
}
