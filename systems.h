#pragma once

#include "components.h"
#include "DebugMarkerSystem.h"
#include "EnemyFollowTargetSystem.h"
#include "EntityExpirationSystem.h"
#include "GameCameraControlSystem.h"
#include "GameMapRenderSystem.h"
#include "GatherInputSystem.h"
#include "PhysicsSystem.h"
#include "PlayerControlSystem.h"
#include "PlayerShootControlSystem.h"
#include "SpriteFacingSystem.h"
#include "SpriteRenderSystem.h"
#include "ViewSystem.h"

struct PhysicsBodyVelocitySystem : TypedSystem<PhysicsBodyVelocitySystem, Velocity, PhysicsBody>
{
	void Update(const GameTime& time) const
	{
		for (Entity entity : entities)
		{
			auto [velocity, body] = GetWorld().GetComponents<Velocity, PhysicsBody>(entity);
			body.velocity = velocity.velocity * time.dt();
		}
	}
};

struct PhysicsNudgeSystem : TypedSystem<PhysicsNudgeSystem, Transform, PhysicsNudge, PhysicsBody>
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
			auto [transform0, nudge0, body0] = GetWorld().GetComponents<Transform, PhysicsNudge, PhysicsBody>(entity0);

			for (int j = i + 1; j < static_cast<int>(entityVector.size()); ++j)
			{
				Entity entity1 = entityVector[j];
				auto [transform1, nudge1, body1] = GetWorld().GetComponents<Transform, PhysicsNudge, PhysicsBody>(entity1);

				Vec2 delta = transform1.position - transform0.position;
				float dist = vec2::Length(delta);
				if (dist <= nudge0.radius + nudge1.radius)
				{
					Vec2 dir = (dist > 0) ? delta / dist : vec2::UnitX;
					nudge0.velocity = nudge0.velocity - dir * nudge1.strength;
					nudge1.velocity = nudge1.velocity + dir * nudge0.strength;
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