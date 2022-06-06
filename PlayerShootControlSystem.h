#pragma once

#include "ecs.h"
#include "types.h"

struct PlayerShootControlSystem : System
{
	void Update(const GameTime& time) const;
};
