#pragma once

#include "draw.h"
#include "ecs.h"

struct DebugMarkerSystem : System
{
	void DrawMarkers(const DrawContext& ctx) const;
};
