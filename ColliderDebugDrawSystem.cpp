#include "ColliderDebugDrawSystem.h"

#include "components.h"
#include "ViewSystem.h"

void ColliderDebugDrawSystem::DrawMarkers(const DrawContext& ctx) const
{
	const auto& viewSystem = GetWorld().GetSystem<ViewSystem>();

	for (Entity entity : entities)
	{
		auto [transform, box] = GetArchetype(entity);

		Vec2 screenPos = viewSystem->WorldToScreen(transform.position + box.center);
		Vec2 screenExtents = viewSystem->WorldScaleToScreen(box.extents);

		Bounds2D colliderBounds = Bounds2D::FromCenter(screenPos, screenExtents);

		draw::Rect(ctx, colliderBounds.min, colliderBounds.max, Color{0, 255, 0, 255});
	}
}
