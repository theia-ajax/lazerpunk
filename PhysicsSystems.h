#pragma once

#include "components.h"
#include "ecs.h"
#include "gamemap.h"
#include "types.h"

struct GameMap;

struct PhysicsSystem final : System<PhysicsSystem, Transform, PhysicsBody>
{
	void SetMap(GameMapHandle mapHandle);
	void Update(const GameTime& time) const;
	bool MapSolid(const Vec2& point) const;
	bool MapSolid(const Bounds2D& bounds, const Vec2& velocity = vec2::Zero) const;

private:
	GameMapHandle activeMapHandle{};
	std::optional<std::reference_wrapper<GameMap>> activeMap;
	std::optional<std::reference_wrapper<GameMapTileLayer>> activeSolidLayer;
};

struct PhysicsBodyVelocitySystem final : System<PhysicsBodyVelocitySystem, Velocity, PhysicsBody>
{
	void Update(const GameTime& time) const;
};

struct PhysicsNudgeSystem final : System<PhysicsNudgeSystem, Transform, PhysicsNudge, PhysicsBody>
{
	void Update(const GameTime& time);
};
