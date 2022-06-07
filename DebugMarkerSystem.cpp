#include "DebugMarkerSystem.h"

#include "components.h"
#include "ViewSystem.h"

void DebugMarkerSystem::DrawMarkers(const DrawContext& ctx) const
{
	const auto& viewSystem = GetWorld().GetSystem<ViewSystem>();

	for (Entity entity : entities)
	{
		auto [transform, marker] = GetWorld().GetComponents<Transform, DebugMarker>(entity);

		if (marker.color.a > 0)
		{
			Vec2 screenPos = viewSystem->WorldToScreen(transform.position);
			draw::Point(ctx, screenPos, marker.color);
		}
	}
}
