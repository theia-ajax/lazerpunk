#include "PlayerShootControlSystem.h"

#include "components.h"

void PlayerShootControlSystem::Update(const GameTime& time) const
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
