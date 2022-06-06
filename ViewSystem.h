#pragma once

#include "ecs.h"
#include "types.h"

struct ViewSystem : System
{
	Entity activeCameraEntity = kInvalidEntity;

	void Update(const GameTime& time);
	Vec2 WorldScaleToScreen(Vec2 worldScale) const;
	Vec2 WorldToScreen(Vec2 worldPosition) const;

	constexpr const Camera& ActiveCamera() const { return activeCamera; }

private:
	Camera activeCamera{};
};

