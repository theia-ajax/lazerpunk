#pragma once

#include "components.h"
#include "ecs.h"

struct SpriteFacingSystem : System<SpriteFacingSystem, Facing, FacingSprites, SpriteRender>
{
	void Update() const;
};

