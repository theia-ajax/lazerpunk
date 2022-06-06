#pragma once

#include "ecs.h"
#include "types.h"

struct EntityExpirationSystem : System
{
	void Update(const GameTime& time) const;
};
