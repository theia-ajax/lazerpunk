#pragma once

#include "draw.h"
#include "ecs.h"

struct SpriteRenderSystem : System
{
	void Render(const DrawContext& ctx) const;
};
