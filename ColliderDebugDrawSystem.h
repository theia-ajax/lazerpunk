#pragma once

#include "components.h"
#include "draw.h"
#include "ecs.h"

struct ColliderDebugDrawSystem : System<ColliderDebugDrawSystem, Transform, Collider::Box>
{
	void DrawMarkers(const DrawContext& ctx) const;
};
