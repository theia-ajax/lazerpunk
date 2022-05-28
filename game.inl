#pragma once

namespace Direction
{
	enum F
	{
		Invalid = -1,
		Left = 0, Right, Up, Down, Count
	};
}

struct GameState
{
	Camera camera;
	SpriteSheet sprites{};

	bool moveDown[Direction::Count] = {};
	float moveDownTimestamp[Direction::Count] = {};

	Vec2 playerPos{0, 0};
	int playerFacing = Direction::Right;
};

namespace game
{
	void Init(GameState& game)
	{
		
	}

	float Length(const Vec2& v) { return v.x * v.x + v.y * v.y; }
	Vec2 Normalize(const Vec2& v)
	{
		float l = Length(v);
		if (l == 0.0f)
		{
			return Vec2{ 0.0f, 0.0f };
		}
		return Vec2{ v.x / l, v.y / l };
	}
	inline Vec2 operator+(const Vec2& lhs, const Vec2& rhs)
	{
		return Vec2{ lhs.x + rhs.x, lhs.y + rhs.y };
	}
	inline Vec2 operator*(const Vec2& lhs, float rhs)
	{
		return Vec2{ lhs.x * rhs, lhs.y * rhs };
	}

	void DirectionInput(bool down[Direction::Count], float timestamp[Direction::Count], SDL_Scancode key, int direction, float time)
	{
		down[direction] = input::GetKey(key);
		if (input::GetKeyDown(key)) timestamp[direction] = time;
	}

	void Update(GameState& game, const GameTime& time)
	{
		DirectionInput(game.moveDown, game.moveDownTimestamp, SDL_SCANCODE_LEFT, Direction::Left, time.t());
		DirectionInput(game.moveDown, game.moveDownTimestamp, SDL_SCANCODE_RIGHT, Direction::Right, time.t());
		DirectionInput(game.moveDown, game.moveDownTimestamp, SDL_SCANCODE_UP, Direction::Up, time.t());
		DirectionInput(game.moveDown, game.moveDownTimestamp, SDL_SCANCODE_DOWN, Direction::Down, time.t());

		int direction = Direction::Invalid;
		float latestTime = 0.0f;
		for (int i = 0; i < Direction::Count; ++i)
		{
			if (game.moveDown[i] && game.moveDownTimestamp[i] > latestTime)
			{
				direction = i;
				latestTime = game.moveDownTimestamp[i];
			}
		}

		Vec2 moveVectors[5] = {
			{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}
		};
		Vec2 moveInput = moveVectors[direction + 1];

		Vec2 velocity = moveInput * time.dt() * 10.0f;
		game.playerPos = game.playerPos + velocity;

		if (direction != Direction::Invalid)
			game.playerFacing = direction;
	}

	void Render(const GameState& game, const GameTime& time)
	{
		Vec2 screenPos = camera::WorldToScreen(game.camera, game.playerPos);

		int spriteId;
		switch (game.playerFacing)
		{
		default:
		case Direction::Left:
		case Direction::Right:
			spriteId = 1043;
			break;
		case Direction::Up:
			spriteId = 1042;
			break;
		case Direction::Down:
			spriteId = 1041;
			break;
		}

		SpriteFlipFlags flip = SpriteFlipFlags::None;
		if (game.playerFacing == Direction::Left)
			FlagSet(flip, SpriteFlipFlags::FlipX, true);

		sprite::Draw(game.sprites, spriteId, screenPos.x, screenPos.y, 0.0f, flip, 0.5f, 0.5f);
		SDL_SetRenderDrawColor(game.sprites._renderer, 255, 0, 0, 255);
		SDL_RenderDrawPointF(game.sprites._renderer, screenPos.x, screenPos.y);
	}
}