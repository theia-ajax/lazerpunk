#include "SpriteFacingSystem.h"

#include "components.h"
#include "ecs.h"

void SpriteFacingSystem::Update() const
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
