#include "PhysicsSystem.h"

#include "components.h"

void PhysicsSystem::Update(const GameTime& time) const
{
	if (!mapHandle)
		return;

	for (Entity entity : entities)
	{
		auto [transform, velocity, collider] = GetWorld().GetComponents<Transform, Velocity, Collider>(entity);

		GameMap& map = map::Get(mapHandle);

		if (MapSolid(map, transform.position))
		{
			auto& marker = GetWorld().GetOrAddComponent(entity, DebugMarker{});
			marker.color = { 255, 0,0, 255 };
		}
		else
		{
			auto& marker = GetWorld().GetOrAddComponent(entity, DebugMarker{});
			marker.color = { 0, 0,0, 0 };
		}
	}
}

bool PhysicsSystem::MapSolid(GameMap& map, const Vec2& point) const
{
	if (!map.worldBounds.ContainsPoint(point))
		return false;

	// TODO: not this miserably slow thing
	std::optional<GameMapLayer> layer = map::GetLayer(map, "Tile Layer 1");

	if (!layer.has_value())
		return false;

	if (const GameMapTileLayer* tileLayer = std::get_if<GameMapTileLayer>(&layer.value()))
	{
		auto [pointX, pointY] = point;
		int px = math::FloorToInt(pointX);
		int py = math::FloorToInt(pointY);
		int index = px + py * tileLayer->tileCountX;
		GameMapTile tile = tileLayer->tiles[index];

		if (tile.tileGlobalId == 588)
			return true;
	}

	return false;
}
