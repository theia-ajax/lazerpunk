#pragma once

#include "components.h"
#include "ControllerSystems.h"
#include "CoreSystems.h"
#include "DrawingSystems.h"
#include "GatherInputSystem.h"
#include "PhysicsSystem.h"

struct PhysicsBodyVelocitySystem : System<PhysicsBodyVelocitySystem, Velocity, PhysicsBody>
{
	void Update(const GameTime& time) const
	{
		for (Entity entity : entities)
		{
			auto [velocity, body] = GetArchetype(entity);
			body.velocity = velocity.velocity * time.dt();
		}
	}
};

struct PhysicsNudgeSystem : System<PhysicsNudgeSystem, Transform, PhysicsNudge, PhysicsBody>
{
	void Update(const GameTime& time) const
	{
		for (Entity entity : entities)
		{
			GetWorld().GetComponent<PhysicsNudge>(entity).velocity = vec2::Zero;
		}

		for (int i = 0; i < static_cast<int>(entityVector.size()) - 1; ++i)
		{
			Entity entity0 = entityVector[i];
			auto [transform0, nudge0, body0] = GetArchetype(entity0);

			for (int j = i + 1; j < static_cast<int>(entityVector.size()); ++j)
			{
				Entity entity1 = entityVector[j];
				auto [transform1, nudge1, body1] = GetArchetype(entity1);

				Vec2 delta = transform1.position - transform0.position;
				float dist = vec2::Length(delta);
				float totalRadius = nudge0.radius + nudge1.radius;
				if (dist <= nudge0.radius + nudge1.radius)
				{
					Vec2 dir = (dist > 0) ? delta / dist : vec2::UnitX;

					float ratio = dist / totalRadius;
					float strength0 = (nudge0.maxStrength > nudge0.minStrength) ? math::lerp(nudge0.maxStrength, nudge0.minStrength, ratio) : nudge0.minStrength;
					float strength1 = (nudge1.maxStrength > nudge1.minStrength) ? math::lerp(nudge1.maxStrength, nudge1.minStrength, ratio) : nudge1.minStrength;

					nudge0.velocity = nudge0.velocity - dir * strength1;
					nudge1.velocity = nudge1.velocity + dir * strength0;
				}
			}
		}

		for (Entity entity : entities)
		{
			auto [nudge, body] = GetWorld().GetComponents<PhysicsNudge, PhysicsBody>(entity);
			body.velocity = body.velocity + nudge.velocity * time.dt();
		}

	}

	void OnEntityAdded(Entity entity) override
	{
		entityVector.emplace_back(entity);
	}

	void OnEntityRemoved(Entity entity) override
	{
		static_cast<void>(std::ranges::remove_if(entityVector, [entity](auto e) { return e == entity; }));
	}


private:
	std::vector<Entity> entityVector;
};

struct SpawnerSystem : System<SpawnerSystem, Transform, Spawner>
{
	
};