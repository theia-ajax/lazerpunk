#pragma once

#include "ecs.h"
#include "types.h"

struct GameCameraControlSystem : System
{
	void SnapFocusToFollow(Entity cameraEntity) const;
	void Update(const GameTime& time) const;
};
