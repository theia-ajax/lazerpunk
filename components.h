#pragma once

#include "types.h"
#include "ecs.h"
#include "sprites.h"
#include "gamemap.h"

struct Transform
{
	Vec2 position = vec2::Zero;
	Vec2 scale = vec2::One;
	float rotation = 0.0f;
};

struct Child
{
	Entity parent;
};

struct CameraView
{
	Vec2 extents{};
	float scale = 16.0f;
	Vec2 center{};
};

namespace camera_view
{
	inline Vec2 WorldExtents(const CameraView& view) { return view.extents / view.scale; }
}

struct GameInputGather
{
	enum_array<bool, Direction> moveDown{};
	enum_array<float, Direction> moveDownTimestamp{};
};

struct GameInput
{
	Direction direction{};
	Vec2 moveInput{};
	bool requestDash{};
	bool requestShoot{};
};

struct Facing
{
	Direction facing{};
};

struct Velocity
{
	Vec2 velocity{};
};

using PhysicsLayer = uint16_t;

struct PhysicsBody
{
	Vec2 velocity{};
	PhysicsLayer layer{};
};

struct Expiration
{
	float secRemaining{};
};

struct PlayerControl
{
	Vec2 velocity{};
	Vec2 dashVelocity{};
	float minDashThreshold = 0.1f;
};

struct PlayerShootControl
{
	float cooldownSec{};
	float cooldownRemaining{};
};

struct EnemyTag
{
	
};

struct FacingSprites
{
	int sideId{};
	int upId{};
	int downId{};
};

struct SpriteRender
{
	int spriteId{};
	SpriteFlipFlags flipFlags{};
	Vec2 origin{};
};

struct GameMapRender
{
	GameMapHandle mapHandle;
};

struct GameCameraControl
{
	GameMapHandle clampViewMap{};
	Entity followTarget{};
	Bounds2D followBounds;
};


struct Collider
{
	struct Box
	{
		Vec2 center{};
		Vec2 extents = vec2::Half;
	};

	struct Circle
	{
		Vec2 center{};
		float radius = 0.5f;
	};
};

struct DebugMarker
{
	Color color{};
};

struct PhysicsNudge
{
	float radius = 0.5f;
	float minStrength = 0.01f;
	float maxStrength{};
	Vec2 velocity{};
};

struct Spawner
{
	Entity prefab{};
	float interval{};
	float spawnTimer{};
	static_stack<Entity, 4> spawnedEnemies{};
};
