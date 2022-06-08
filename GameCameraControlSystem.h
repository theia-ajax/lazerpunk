#pragma once

#include "components.h"
#include "ecs.h"
#include "types.h"

struct GameCameraControlSystem : System<GameCameraControlSystem, Transform, CameraView, GameCameraControl>
{
	void SnapFocusToFollow(Entity cameraEntity) const;
	void Update(const GameTime& time) const;
};
