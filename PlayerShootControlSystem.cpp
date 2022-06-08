#include "PlayerShootControlSystem.h"

#include "components.h"

void PlayerShootControlSystem::Update(const GameTime& time) const
{
	for (Entity entity : entities)
	{
		auto [input, transform, facing, velocity, shootControl] = GetArchetype(entity);

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
					PhysicsBody{},
					Facing{ facing.facing },
					SpriteRender{ 14	, flags, vec2::Half },
					Expiration{ 1.0f });
			}
		}
	}
}
