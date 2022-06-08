#include "DebugMarkerSystem.h"

#include "components.h"
#include "ViewSystem.h"

void DebugMarkerSystem::DrawMarkers(const DrawContext& ctx) const
{
	const auto& viewSystem = GetWorld().GetSystem<ViewSystem>();

	for (Entity entity : entities)
	{
		auto [transform, marker, box] = GetArchetype(entity);

		if (marker.color.a > 0)
		{
			Vec2 screenPos = viewSystem->WorldToScreen(transform.position + box.center);
			Vec2 screenExtents = viewSystem->WorldScaleToScreen(box.extents);

			Bounds2D colliderBounds = Bounds2D::FromCenter(screenPos, screenExtents);

			draw::Rect(ctx, colliderBounds.min, colliderBounds.max, marker.color);
		}
	}
}
