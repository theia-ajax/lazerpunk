#include "PhysicsSystem.h"

#include <algorithm>
#include "components.h"

void PhysicsSystem::SetMap(GameMapHandle handle)
{
	activeMapHandle = handle;
	if (activeMapHandle)
	{
		activeMap = map::Get(activeMapHandle);
		activeSolidLayer = map::GetLayer<GameMapTileLayer>(activeMap.value(), "Tile Layer 1");
	}
}

void PhysicsSystem::Update(const GameTime& time) const
{
	for (Entity entity : entities)
	{
		auto [transform, body, collider] = GetWorld().GetComponents<Transform, PhysicsBody, Collider::Box>(entity);

		Vec2 origin = transform.position;
		Vec2 startVel = body.velocity;
		Vec2 vel = startVel;

		Bounds2D colliderBounds = Bounds2D::FromCenter(transform.position + collider.center, collider.extents);
		const auto& corners = colliderBounds.Corners();

		auto [velX, velY] = vec2::UnitVectors(vel);

		while (!vec2::ApproxZero(velX) &&
			std::ranges::any_of(corners, [&](auto&& p) { return MapSolid(p + velX); }))
		{
			velX.x = math::MoveTo(velX.x, 0.0f, 0.0625f);
		}

		while (!vec2::ApproxZero(velY) &&
			std::ranges::any_of(corners, [&](auto&& p) { return MapSolid(p + velY); }))
		{
			velY.y = math::MoveTo(velY.y, 0.0f, 0.0625f);
		}

		vel.x = velX.x;
		vel.y = velY.y;

		body.velocity = vel;

		transform.position = transform.position + body.velocity;

		if (std::ranges::any_of(corners, [&](auto&& p) { return MapSolid(p); }))
		{
			auto& [color] = GetWorld().GetOrAddComponent(entity, DebugMarker{});
			color = color::RGB(255, 0, 255);
		}
		else
		{
			auto& marker = GetWorld().GetOrAddComponent(entity, DebugMarker{});
			marker.color = color::RGB(0, 255, 255);
		}
	}
}

bool PhysicsSystem::MapSolid(const Vec2& point) const
{
	if (!(activeMap.has_value() && activeSolidLayer.has_value()))
		return false;

	GameMap& map = activeMap.value();

	if (!map.worldBounds.ContainsPoint(point))
		return false;

	GameMapTileLayer& solidLayer = activeSolidLayer.value();

	auto [pointX, pointY] = point;
	int px = math::FloorToInt(pointX);
	int py = math::FloorToInt(pointY);
	int index = px + py * solidLayer.tileCountX;
	auto [tileGlobalId, position, flipFlags] = solidLayer.tiles[index];

	return (tileGlobalId == 588 || tileGlobalId == 149);
}
