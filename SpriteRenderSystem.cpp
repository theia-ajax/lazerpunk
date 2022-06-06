#include "SpriteRenderSystem.h"

#include "components.h"
#include "ViewSystem.h"

void SpriteRenderSystem::Render(const DrawContext& ctx) const
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
