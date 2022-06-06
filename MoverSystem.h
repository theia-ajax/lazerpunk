#pragma once

#include "ecs.h"
#include "types.h"

struct MoverSystem : System
{
	void Update(const GameTime& time) const;
};
