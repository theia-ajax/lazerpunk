#include "PhysicsSystems.h"

#include <algorithm>
#include <sokol_time.h>

#include "components.h"
#include "debug.h"

void PhysicsSystem::SetMap(GameMapHandle handle)
{
	activeMapHandle = handle;
	if (activeMapHandle)
	{
		activeMap = map::Get(activeMapHandle);
		activeSolidLayer = map::GetLayer<GameMapTileLayer>(activeMap, "Tile Layer 1");
	}
}

void PhysicsSystem::Update(const GameTime& time)
{
	for (Entity entity : GetEntities())
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
	if (!activeMap || !activeSolidLayer)
		return false;

	if (!activeMap->worldBounds.ContainsPoint(point))
		return false;

	GameMapTileLayer& solidLayer = *activeSolidLayer;

	auto [pointX, pointY] = point;
	int px = math::FloorToInt(pointX);
	int py = math::FloorToInt(pointY);
	int index = px + py * solidLayer.tileCountX;
	auto [tileGlobalId, position, flipFlags] = solidLayer.tiles[index];

	if (tileGlobalId == 0)
		return false;

	int id = tileGlobalId - 1;

	return (id == 7 || id == 58);
}

bool PhysicsSystem::MapSolid(const Bounds2D& bounds, const Vec2& velocity) const
{
	return std::ranges::any_of(bounds.Corners(),
		[this, velocity](auto&& p) { return MapSolid(p + velocity); });
}

void PhysicsBodyVelocitySystem::Update(const GameTime& time)
{
	for (Entity entity : GetEntities())
	{
		auto [velocity, body] = GetArchetype(entity);
		body.velocity = velocity.velocity * time.dt();
	}
}

void PhysicsNudgeSystem::Update(const GameTime& time)
{
	const std::vector<Entity>& entityVector = GetEntities();

	for (Entity entity : entityVector)
	{
		GetWorld().GetComponent<PhysicsNudge>(entity).velocity = vec2::Zero;
	}

	for (int i = 0; i < static_cast<int>(entityVector.size()) - 1; ++i)
	{
		Entity entity0 = entityVector[i];
		auto [transform0, nudge0, body0] = GetArchetype(entity0);

		for (int j = i + 1; j < static_cast<int>(entityVector.size()); ++j)
		{
			Entity entity1 = entityVector[j];
			auto [transform1, nudge1, body1] = GetArchetype(entity1);

			Vec2 delta = transform1.position - transform0.position;
			float dist = vec2::Length(delta);
			float totalRadius = nudge0.radius + nudge1.radius;
			if (dist <= nudge0.radius + nudge1.radius)
			{
				Vec2 dir = (dist > 0) ? delta / dist : vec2::UnitX;

				float ratio = dist / totalRadius;
				float strength0 = (nudge0.maxStrength > nudge0.minStrength) ? math::lerp(nudge0.maxStrength, nudge0.minStrength, ratio) : nudge0.minStrength;
				float strength1 = (nudge1.maxStrength > nudge1.minStrength) ? math::lerp(nudge1.maxStrength, nudge1.minStrength, ratio) : nudge1.minStrength;

				nudge0.velocity = nudge0.velocity - dir * strength1;
				nudge1.velocity = nudge1.velocity + dir * strength0;
			}
		}
	}

	for (Entity entity : entityVector)
	{
		auto [nudge, body] = GetWorld().GetComponents<PhysicsNudge, PhysicsBody>(entity);
		body.velocity = body.velocity + nudge.velocity * time.dt();
	}
}
