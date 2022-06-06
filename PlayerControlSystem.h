#pragma once

#include "ecs.h"
#include "types.h"

struct PlayerControlSystem : System
{
	void Update(const GameTime& time) const;
};