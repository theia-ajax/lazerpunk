#pragma once

#include "components.h"
#include "ecs.h"
#include "types.h"

struct PlayerControlSystem : System<PlayerControlSystem, GameInput, Transform, Facing, Velocity, PlayerControl>
{
	void Update(const GameTime& time) const;
};