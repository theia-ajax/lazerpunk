#pragma once

#include "ecs.h"
#include "types.h"

struct GatherInputSystem : System
{
	void Update(const GameTime& time) const;
};
