#pragma once

#include "components.h"
#include "ecs.h"
#include "types.h"

struct PlayerShootControlSystem : System<PlayerShootControlSystem, GameInput, Transform, Facing, Velocity, PlayerShootControl>
{
	void Update(const GameTime& time) const;
};
