#pragma once

#include "ecs.h"

struct EnemyFollowTargetSystem : System
{
	Entity targetEntity{};
	void Update(const GameTime& time) const;
};
