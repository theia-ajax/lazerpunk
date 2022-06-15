#pragma once

#include "types.h"
#include "ecs.h"
#include "components.h"

struct EnemyFollowTargetSystem : System<EnemyFollowTargetSystem, Transform, Velocity, EnemyTag>
{
	Entity targetEntity{};
	void Update(const GameTime& time);
};

struct GameCameraControlSystem : System<GameCameraControlSystem, Transform, CameraView, GameCameraControl>
{
	void SnapFocusToFollow(Entity cameraEntity) const;
	void Update(const GameTime& time);
};

struct PlayerControlSystem : System<PlayerControlSystem, GameInput, Transform, Facing, Velocity, PlayerControl>
{
	void Update(const GameTime& time);
};

struct PlayerShootControlSystem : System<PlayerShootControlSystem, GameInput, Transform, Facing, Velocity, PlayerShootControl>
{
	void Update(const GameTime& time);
};


struct SpawnerSystem : System<SpawnerSystem, Transform, Spawner>
{
	void OnRegistered() override;
	void Update(const GameTime& time);
	Query<Reject<Prefab>, SpawnSource>* spawnSourceQuery{};
};

