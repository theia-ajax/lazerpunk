#include "DrawingSystems.h"

#include "draw.h"
#include "CoreSystems.h"

void ColliderDebugDrawSystem::DrawMarkers(const DrawContext& ctx)
{
	const auto& viewSystem = GetWorld().GetSystem<ViewSystem>();

	for (Entity entity : GetEntities())
	{
		auto [transform, box] = GetArchetype(entity);

		Vec2 screenPos = viewSystem->WorldToScreen(transform.position + box.center);
		Vec2 screenExtents = viewSystem->WorldScaleToScreen(box.extents);

		Bounds2D colliderBounds = Bounds2D::FromCenter(screenPos, screenExtents);

		draw::Rect(ctx, colliderBounds.min, colliderBounds.max, Color{ 0, 255, 0, 255 });
	}
}

void GameMapRenderSystem::RenderLayers(const DrawContext& ctx, const StrId* layers, size_t count)
{
	const auto& viewSystem = GetWorld().GetSystem<ViewSystem>();
	for (Entity entity : GetEntities())
	{
		auto [transform, mapRender] = GetArchetype(entity);
		if (mapRender.mapHandle)
		{
			if (GameMap* map = map::Get(mapRender.mapHandle))
				map::DrawLayers(ctx, *map, viewSystem->ActiveCamera(), ctx.sheet, layers, count);
		}
	}
}

void SpriteFacingSystem::Update()
{
	for (auto& entities = GetEntities(); Entity entity : entities)
	{
		switch (auto [facing, facingSprites, sprite] = GetArchetype(entity); facing.facing)
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

void SpriteRenderSystem::Render(const DrawContext& ctx)
{
	const auto& viewSystem = GetWorld().GetSystem<ViewSystem>();

	for (Entity entity : GetEntities())
	{
		auto [transform, sprite] = GetArchetype(entity);
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
