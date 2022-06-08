#pragma once

#include "components.h"
#include "draw.h"
#include "ecs.h"

struct DebugMarkerSystem : System<DebugMarkerSystem, Transform, DebugMarker, Collider::Box>
{
	void DrawMarkers(const DrawContext& ctx) const;
};
