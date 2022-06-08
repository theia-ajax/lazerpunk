#include "GatherInputSystem.h"
#include <SDL2/SDL.h>
#include "components.h"
#include "input.h"

namespace
{
	void DirectionInput(enum_array<bool, Direction>& down, enum_array<float, Direction>& timestamp, SDL_Scancode key, Direction direction, float time)
	{
		down[direction] = input::GetKey(key);
		if (input::GetKeyDown(key)) timestamp[direction] = time;
	}
}

void GatherInputSystem::Update(const GameTime& time) const
{
	for (Entity entity : entities)
	{
		auto [gather, gameInput] = GetArchetype(entity);

		DirectionInput(gather.moveDown, gather.moveDownTimestamp, SDL_SCANCODE_LEFT, Direction::Left, time.t());
		DirectionInput(gather.moveDown, gather.moveDownTimestamp, SDL_SCANCODE_RIGHT, Direction::Right, time.t());
		DirectionInput(gather.moveDown, gather.moveDownTimestamp, SDL_SCANCODE_UP, Direction::Up, time.t());
		DirectionInput(gather.moveDown, gather.moveDownTimestamp, SDL_SCANCODE_DOWN, Direction::Down, time.t());


		Direction direction = Direction::Invalid;
		float latestTime = 0.0f;
		for (Direction dir = Direction::Left; dir < Direction::Count; ++dir)
		{
			if (gather.moveDown[dir] && gather.moveDownTimestamp[dir] > latestTime)
			{
				direction = dir;
				latestTime = gather.moveDownTimestamp[dir];
			}
		}

		gameInput.moveInput = DirectionVector(direction);
		gameInput.direction = direction;

		if (input::GetKeyDown(SDL_SCANCODE_X))
		{
			gameInput.requestDash = true;
		}

		gameInput.requestShoot = input::GetKey(SDL_SCANCODE_Z);
	}
}

