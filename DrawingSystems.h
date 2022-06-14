#pragma once

#include "components.h"
#include "ecs.h"
#include "types.h"

struct ColliderDebugDrawSystem : System<ColliderDebugDrawSystem, Transform, Collider::Box>
{
	void DrawMarkers(const DrawContext& ctx);
};

struct GameMapRenderSystem : System<GameMapRenderSystem, Transform, GameMapRender>
{
	void RenderLayers(const DrawContext& ctx, const StrId* layers, size_t count);

	template <int N>
	void RenderLayers(const DrawContext& ctx, const std::array<StrId, N>& layers)
	{
		RenderLayers(ctx, layers.data(), layers.size());
	}
};

struct SpriteFacingSystem : System<SpriteFacingSystem, Facing, FacingSprites, SpriteRender>
{
	void Update();
};

struct SpriteRenderSystem : System<SpriteRenderSystem, Transform, SpriteRender>
{
	void Render(const DrawContext& ctx);
};
