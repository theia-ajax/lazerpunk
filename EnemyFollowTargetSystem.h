#pragma once

#include "components.h"
#include "ecs.h"

struct EnemyFollowTargetSystem : System<EnemyFollowTargetSystem, Transform, Velocity, EnemyTag>
{
	Entity targetEntity{};
	void Update(const GameTime& time) const;
};
