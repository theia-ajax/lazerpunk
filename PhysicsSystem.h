#pragma once

#include "ecs.h"
#include "gamemap.h"
#include "types.h"

struct GameMap;

struct PhysicsSystem : System
{
	void SetMap(GameMapHandle mapHandle);
	void Update(const GameTime& time) const;
	bool MapSolid(const Vec2& point) const;

private:
	GameMapHandle activeMapHandle{};
	std::optional<std::reference_wrapper<GameMap>> activeMap;
	std::optional< std::reference_wrapper<GameMapTileLayer>> activeSolidLayer;
};

