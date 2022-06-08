#pragma once

#include "components.h"
#include "draw.h"
#include "ecs.h"

struct SpriteRenderSystem : System<SpriteRenderSystem, Transform, SpriteRender>
{
	void Render(const DrawContext& ctx) const;
};
