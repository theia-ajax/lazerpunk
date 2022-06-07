#pragma once

#include "ecs.h"
#include "gamemap.h"
#include "types.h"

struct GameMap;

struct PhysicsSystem : System
{
	GameMapHandle mapHandle{};
	void Update(const GameTime& time) const;
	bool MapSolid(GameMap& map, const Vec2& point) const;
};

