#pragma once

#include "components.h"
#include "ecs.h"
#include "types.h"

struct GatherInputSystem : System<GatherInputSystem, GameInputGather, GameInput>
{
	void Update(const GameTime& time) const;
};
