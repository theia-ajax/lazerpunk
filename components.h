#pragma once

#include "types.h"
#include "ecs.h"
#include "sprites.h"
#include "gamemap.h"

// Core
struct DestroyEntityTag {};

struct Expiration
{
	float secRemaining{};
};

struct Transform
{
	Vec2 position = vec2::Zero;
	Vec2 scale = vec2::One;
	float rotation = 0.0f;
};

struct Velocity
{
	Vec2 velocity{};
};

struct Facing
{
	Direction facing{};
};

// Input
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

// Camera
struct CameraView
{
	Vec2 extents{};
	float scale = 16.0f;
	Vec2 center{};
};

namespace camera_view {
	inline Vec2 WorldExtents(const CameraView& view) { return view.extents / view.scale; }
}

struct GameCameraControl
{
	GameMapHandle clampViewMap{};
	Entity followTarget{};
	Bounds2D followBounds;
};

// Player
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

// Enemies
struct EnemyTag
{
	
};

struct Spawner
{
	Entity prefab{};
	float interval{};
	float spawnTimer{};
	int32_t maxAlive{};
	int32_t spawnedEnemies{};
};

struct SpawnSource
{
	Entity source;
};

// Physics/Collision
struct PhysicsBody
{
	Vec2 velocity{};
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

struct PhysicsLayer
{
	uint16_t layer;
};

struct Trigger
{
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

// Rendering
struct FacingSprites
{
	int16_t sideId{};
	int16_t upId{};
	int16_t downId{};
};

struct SpriteRender
{
	int16_t spriteId{};
	SpriteFlipFlags flipFlags{};
	Vec2 origin{};
};

struct GameMapRender
{
	GameMapHandle mapHandle;
};
