#pragma once

#include "ecs.h"
#include "draw.h"
#include "types.h"

struct GameMapRenderSystem : System
{
	void RenderLayers(const DrawContext& ctx, const StrId* layers, size_t count) const;

	template <int N>
	void RenderLayers(const DrawContext& ctx, const std::array<StrId, N>& layers)
	{
		RenderLayers(ctx, layers.data(), layers.size());
	}
};
