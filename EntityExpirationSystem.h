#pragma once

#include "components.h"
#include "ecs.h"
#include "types.h"

struct EntityExpirationSystem : System<EntityExpirationSystem, Expiration>
{
	void Update(const GameTime& time) const;
};
