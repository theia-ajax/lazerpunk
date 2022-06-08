#include "GameMapRenderSystem.h"

#include "components.h"
#include "gamemap.h"
#include "ViewSystem.h"

void GameMapRenderSystem::RenderLayers(const DrawContext& ctx, const StrId* layers, size_t count) const
{
	const auto& viewSystem = GetWorld().GetSystem<ViewSystem>();
	for (Entity entity : entities)
	{
		auto [transform, mapRender] = GetArchetype(entity);
		if (mapRender.mapHandle)
		{
			const auto& map = map::Get(mapRender.mapHandle);
			map::DrawLayers(ctx, map, viewSystem->ActiveCamera(), ctx.sheet, layers, count);
		}
	}
}
