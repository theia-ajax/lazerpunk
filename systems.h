#pragma once

#include "types.h"
#include "ecs.h"
#include "components.h"
#include "draw.h"

struct GatherInputSystem : System
{
	void Update(const GameTime& time) const;
};

struct PlayerControlSystem : System
{
	void Update(const GameTime& time) const
	{
		using vec2::Length;
		using vec2::Normalize;

		for (Entity entity : entities)
		{
			auto& input = GetWorld().GetComponent<GameInput>(entity);
			auto& transform = GetWorld().GetComponent<Transform>(entity);
			auto& [facing] = GetWorld().GetComponent<Facing>(entity);
			auto& [velocity] = GetWorld().GetComponent<Velocity>(entity);
			auto& control = GetWorld().GetComponent<PlayerControl>(entity);

			if (input.direction != Direction::Invalid)
				facing = input.direction;

			control.velocity = input.moveInput * 10.0f;

			if (input.requestDash)
			{
				input.requestDash = false;
				if (Length(control.dashVelocity) < 0.1f)
				{
					control.dashVelocity = DirectionVector(facing) * 25.0f;
				}
			}

			float dashMag = Length(control.dashVelocity);
			if (float newDashMag = math::Damp(dashMag, 0.0f, 5.0f, time.dt()); newDashMag < control.minDashThreshold)
			{
				control.dashVelocity = vec2::Zero;
			}
			else
			{
				control.dashVelocity = Normalize(control.dashVelocity) * newDashMag;
			}

			velocity = control.velocity + control.dashVelocity;
		}
	}
};

struct PlayerShootControlSystem : System
{
	void Update(const GameTime& time) const
	{
		for (Entity entity : entities)
		{
			const auto [input, transform, facing, velocity] = GetWorld().GetComponents<GameInput, Transform, Facing, Velocity>(entity);
			auto& shootControl = GetWorld().GetComponent<PlayerShootControl>(entity);

			if (shootControl.cooldownRemaining > 0)
				shootControl.cooldownRemaining -= time.dt();

			if (input.requestShoot)
			{
				if (shootControl.cooldownRemaining <= 0.0f)
				{
					shootControl.cooldownRemaining += shootControl.cooldownSec;

					Vec2 bulletVel = DirectionVector(facing.facing) * 25;
					SpriteFlipFlags flags{};
					switch (facing.facing)
					{
					case Direction::Left:
						flags = SpriteFlipFlags::FlipDiag;
						break;
					case Direction::Right:
						flags = SpriteFlipFlags::FlipDiag | SpriteFlipFlags::FlipX;
						break;
					case Direction::Down:
						flags = SpriteFlipFlags::FlipY;
						break;
					}
					Entity bulletEntity = GetWorld().CreateEntity();
					GetWorld().AddComponents(bulletEntity,
						Transform{ {transform.position} },
						Velocity{ bulletVel },
						Facing{ facing.facing },
						SpriteRender{ 664, flags, vec2::Half },
						Expiration{ 1.0f });
				}
			}
		}
	}
};

struct SpriteFacingSystem : System
{
	void Update() const
	{
		for (Entity entity : entities)
		{
			const auto& [facing] = GetWorld().GetComponent<Facing>(entity);
			const auto& facingSprites = GetWorld().GetComponent<FacingSprites>(entity);
			auto& sprite = GetWorld().GetComponent<SpriteRender>(entity);

			switch (facing)
			{
			default: break;
			case Direction::Left:
				flags::Set(sprite.flipFlags, SpriteFlipFlags::FlipX, true);
				sprite.spriteId = facingSprites.sideId;
				break;
			case Direction::Right:
				flags::Set(sprite.flipFlags, SpriteFlipFlags::FlipX, false);
				sprite.spriteId = facingSprites.sideId;
				break;
			case Direction::Up:
				sprite.spriteId = facingSprites.upId;
				break;
			case Direction::Down:
				sprite.spriteId = facingSprites.downId;
				break;
			}

		}
	}
};

struct MoverSystem : System
{
	void Update(const GameTime& time) const
	{
		for (Entity entity : entities)
		{
			auto& transform = GetWorld().GetComponent<Transform>(entity);
			auto& [velocity] = GetWorld().GetComponent<Velocity>(entity);
			transform.position = transform.position + velocity * time.dt();
		}
	}
};

struct ViewSystem : System
{
	Entity activeCameraEntity = kInvalidEntity;

	void Update(const GameTime& time)
	{
		activeCameraEntity = 0;

		for (Entity entity : entities)
		{
			if (!activeCameraEntity)
				activeCameraEntity = entity;

			auto [transform, view] = GetWorld().GetComponents<Transform, CameraView>(entity);
			view.center = transform.position + view.extents / 2 / view.scale;
		}

		if (activeCameraEntity)
		{
			const auto& transform = GetWorld().GetComponent<Transform>(activeCameraEntity);
			const auto& cameraView = GetWorld().GetComponent<CameraView>(activeCameraEntity);

			activeCamera.position = transform.position * cameraView.scale;
			activeCamera.extents = cameraView.extents;
			activeCamera.scale = cameraView.scale;
		}
	}

	Vec2 WorldScaleToScreen(Vec2 worldScale) const
	{
		return camera::WorldScaleToScreen(activeCamera, worldScale);
	}

	Vec2 WorldToScreen(Vec2 worldPosition) const
	{
		return camera::WorldToScreen(activeCamera, worldPosition);
	}

	constexpr const Camera& ActiveCamera() const { return activeCamera; }

private:
	Camera activeCamera{};
};

struct GameCameraControlSystem : System
{
	void SnapFocusToFollow(Entity cameraEntity) const
	{
		auto [transform, view, camera] = GetWorld().GetComponents<Transform, CameraView, GameCameraControl>(cameraEntity);

		if (!camera.followTarget)
			return;

		const auto& [position, _, __] = GetWorld().GetComponent<Transform>(camera.followTarget);

		transform.position = position - view.extents / 2 / view.scale;
		view.center = position;
	}

	void Update(const GameTime& time) const
	{
		for (Entity entity : entities)
		{
			auto [transform, view, camera] = GetWorld().GetComponents<Transform, CameraView, GameCameraControl>(entity);

			if (camera.followTarget)
			{
				const auto& targetTransform = GetWorld().GetComponent<Transform>(camera.followTarget);

				auto [dx, dy] = targetTransform.position - view.center;

				if (dx < camera.followBounds.Left())
					transform.position.x += dx - camera.followBounds.Left();
				if (dx > camera.followBounds.Right())
					transform.position.x += dx - camera.followBounds.Right();
				if (dy < camera.followBounds.Top())
					transform.position.y += dy - camera.followBounds.Top();
				if (dy > camera.followBounds.Bottom())
					transform.position.y += dy - camera.followBounds.Bottom();
			}

			if (camera.clampViewMap)
			{
				const auto& gameMap = map::Get(camera.clampViewMap);
				Bounds2D viewBounds = gameMap.worldBounds;
				viewBounds.max = viewBounds.max - camera_view::WorldExtents(view);
				transform.position = viewBounds.ClampPoint(transform.position);
			}
		}
	}
};

struct SpriteRenderSystem : System
{
	void Render(const DrawContext& ctx) const
	{
		const auto& viewSystem = GetWorld().GetSystem<ViewSystem>();

		for (Entity entity : entities)
		{
			const auto& [transform, sprite] = GetWorld().GetComponents<Transform, SpriteRender>(entity);
			Vec2 screenPos = viewSystem->WorldToScreen(transform.position);
			draw::Sprite(ctx,
				ctx.sheet,
				sprite.spriteId,
				screenPos,
				transform.rotation,
				sprite.flipFlags,
				sprite.origin,
				transform.scale);
		}
	}
};

struct GameMapRenderSystem : System
{
	template <int N>
	void RenderLayers(const DrawContext& ctx, const std::array<StrId, N>& layers)
	{
		const auto& viewSystem = GetWorld().GetSystem<ViewSystem>();
		for (Entity entity : entities)
		{
			auto [transform, mapRender] = GetWorld().GetComponents<Transform, GameMapRender>(entity);
			if (mapRender.mapHandle)
			{
				const auto& map = map::Get(mapRender.mapHandle);
				map::DrawLayers<N>(ctx, map, viewSystem->ActiveCamera(), ctx.sheet, layers);
			}
		}
	}
};

struct EntityExpirationSystem : System
{
	void Update(const GameTime& time) const
	{
		std::queue<Entity> removeQueue{};

		for (Entity entity : entities)
		{
			auto& [secRemaining] = GetWorld().GetComponent<Expiration>(entity);

			secRemaining -= time.dt();

			if (secRemaining <= 0)
				removeQueue.push(entity);
		}

		while (!removeQueue.empty())
		{
			GetWorld().DestroyEntity(removeQueue.front());
			removeQueue.pop();
		}
	}
};

struct EnemyFollowTargetSystem : System
{
	Entity targetEntity{};

	void Update(const GameTime& time)
	{
		if (!targetEntity)
			return;

		const auto& targetTransform = GetWorld().GetComponent<Transform>(targetEntity);

		for (Entity entity : entities)
		{
			auto [transform, velocity] = GetWorld().GetComponents<Transform, Velocity>(entity);

			Vec2 delta = targetTransform.position - transform.position;
			Vec2 dir = vec2::Normalize(delta);
			Vec2 vel = vec2::Damp(velocity.velocity, dir * 10, 0.4f, time.dt());
			velocity.velocity = vel;
		}

		for (Entity entity0 : entities)
		{
			auto [tx0, vel0] = GetWorld().GetComponents<Transform, Velocity>(entity0);

			for (Entity entity1 : entities)
			{
				if (entity0 == entity1)
					continue;

				auto [tx1, vel1] = GetWorld().GetComponents<Transform, Velocity>(entity1);

				Vec2 delta = tx1.position - tx0.position;
				float dist = vec2::Length(delta);
				Vec2 dir = vec2::Normalize(delta);
				if (vec2::Length(delta) < 1.0f)
				{
					vel0.velocity = vel0.velocity - dir * 0.01f;
					vel1.velocity = vel1.velocity + dir * 0.01f;
				}
			}
		}
	}
};