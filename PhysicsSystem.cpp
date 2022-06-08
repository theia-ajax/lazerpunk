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
		auto [transform, body] = GetArchetype(entity);

		Vec2 origin = transform.position;

		auto calculateSolid = [this](const Transform& t, Vec2 velocity, const Collider::Box& collider) -> std::pair<bool, Vec2>
		{
			Bounds2D colliderBounds = Bounds2D::FromCenter(t.position + collider.center, collider.extents);

			auto [velX, velY] = vec2::UnitVectors(velocity);
			bool foundSolid = false;

			while (!vec2::ApproxZero(velX) && MapSolid(colliderBounds, velX))
			{
				velX.x = math::MoveTo(velX.x, 0.0f, 0.0625f);
				foundSolid = true;
			}

			while (!vec2::ApproxZero(velY) && MapSolid(colliderBounds, velY))
			{
				velY.y = math::MoveTo(velY.y, 0.0f, 0.0625f);
				foundSolid = true;
			}

			velocity.x = velX.x;
			velocity.y = velY.y;
			while (!vec2::ApproxZero(velocity) && MapSolid(colliderBounds, velocity))
			{
				velocity = vec2::MoveTo(velocity, vec2::Zero, 0.0625f);
				foundSolid = true;
			}

			return {foundSolid, velocity};
		};
		
		if (auto collider = GetWorld().GetOptionalComponent<Collider::Box>(entity); collider.has_value())
		{
			if (auto [foundSolid, newVelocity] = calculateSolid(transform, body.velocity, collider.value()); foundSolid)
			{
				body.velocity = newVelocity;

				auto& [color] = GetWorld().GetOrAddComponent(entity, DebugMarker{});
				color = color::RGB(255, 0, 255);
			}
			else
			{
				auto& marker = GetWorld().GetOrAddComponent(entity, DebugMarker{});
				marker.color = color::RGB(0, 255, 255);
			}
		}

		transform.position = transform.position + body.velocity;

	}
}

bool PhysicsSystem::MapSolid(const Vec2& point) const
{
	if (!(activeMap.has_value() && activeSolidLayer.has_value()))
		return false;

	if (GameMap& map = activeMap.value(); !map.worldBounds.ContainsPoint(point))
		return false;

	GameMapTileLayer& solidLayer = activeSolidLayer.value();

	auto [pointX, pointY] = point;
	int px = math::FloorToInt(pointX);
	int py = math::FloorToInt(pointY);
	int index = px + py * solidLayer.tileCountX;
	auto [tileGlobalId, position, flipFlags] = solidLayer.tiles[index];

	return (tileGlobalId == 588 || tileGlobalId == 149);
}

bool PhysicsSystem::MapSolid(const Bounds2D& bounds, const Vec2& velocity) const
{
	return std::ranges::any_of(bounds.Corners(),
		[this, velocity](auto&& p) { return MapSolid(p + velocity); });
}