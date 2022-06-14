#pragma once

#include "components.h"
#include "ecs.h"
#include "types.h"

struct EntityExpirationSystem : System<EntityExpirationSystem, Expiration>
{
	void Update(const GameTime& time);
};

struct ViewSystem : System<ViewSystem, Transform, CameraView>
{
	Entity activeCameraEntity = kInvalidEntity;

	void Update(const GameTime& time);
	Vec2 WorldScaleToScreen(Vec2 worldScale) const;
	Vec2 WorldToScreen(Vec2 worldPosition) const;

	constexpr const Camera& ActiveCamera() const { return activeCamera; }

private:
	Camera activeCamera{};
};

