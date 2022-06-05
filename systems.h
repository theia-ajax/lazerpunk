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
					control.dashVelocity = DirectionVelocity(facing) * 25.0f;
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

					Entity bulletEntity = GetWorld().CreateEntity();
					GetWorld().AddComponents(bulletEntity,
							Transform{{transform.position}},
							Velocity{vec2::UnitX},
							Facing{facing.facing},
							SpriteRender{644},
							Expiration{2.0f});
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

struct CameraFollowSystem final : System
{
	void SnapFocusToFollow(Entity cameraEntity) const
	{
		auto [transform, view, follow] = GetWorld().GetComponents<Transform, CameraView, CameraFollowEntity>(cameraEntity);

		if (!follow.target)
			return;

		const auto& [position, _, __] = GetWorld().GetComponent<Transform>(follow.target);

		transform.position = position - view.extents / 2 / view.scale;
		view.center = position;
	}

	void Update(const GameTime& time) const
	{
		for (Entity entity : entities)
		{
			auto [transform, view, follow] = GetWorld().GetComponents<Transform, CameraView, CameraFollowEntity>(entity);

			if (follow.target == kInvalidEntity)
				continue;

			const auto& targetTransform = GetWorld().GetComponent<Transform>(follow.target);

			auto [dx, dy] = targetTransform.position - view.center;

			if (dx < follow.bounds[0].x)
				transform.position.x += dx - follow.bounds[0].x;
			if (dx > follow.bounds[1].x)
				transform.position.x += dx - follow.bounds[1].x;
			if (dy < follow.bounds[0].y)
				transform.position.y += dy - follow.bounds[0].y;
			if (dy > follow.bounds[1].y)
				transform.position.y += dy - follow.bounds[1].y;
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
			const auto& [mapHandle] = GetWorld().GetComponent<GameMapRef>(entity);
			const auto& map = map::Get(mapHandle);
			map::DrawLayers<N>(ctx, map, viewSystem->ActiveCamera(), ctx.sheet, layers);
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