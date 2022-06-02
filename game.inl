#pragma once

//namespace Direction
//{
//	enum Direction
//	{
//		Invalid = 0,
//		Left, Right, Up, Down, Count
//	};
//}
//

enum class Direction
{
	Invalid,
	Left,
	Right,
	Up,
	Down,
	Count,
};

constexpr int kDirectionCount = static_cast<int>(Direction::Count);




struct GameState
{
	Camera camera;
	SpriteSheet sprites{};
	GameMap map;

	enum_array<bool, Direction> moveDown{};
	enum_array<float, Direction> moveDownTimestamp{};

	Vec2 playerPos{0, 0};
	Direction playerFacing = Direction::Right;
	Vec2 moveInput{};
	bool requestDash{};
	Vec2 dashVelocity{};
	float dashDecay{ 0.05f };
};

namespace game
{
	void Init(GameState& game)
	{
		game.playerPos = { 8, 5 };
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
	float Dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }

	void DirectionInput(enum_array<bool, Direction>& down, enum_array<float, Direction>& timestamp, SDL_Scancode key, Direction direction, float time)
	{
		down[direction] = input::GetKey(key);
		if (input::GetKeyDown(key)) timestamp[direction] = time;
	}

	inline Vec2 DirectionVelocity(Direction direction)
	{
		Vec2 moveVectors[kDirectionCount] = {
			{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}
		};
		return moveVectors[static_cast<int>(direction)];
	}

	void Update(GameState& game, const GameTime& time)
	{
		DirectionInput(game.moveDown, game.moveDownTimestamp, SDL_SCANCODE_LEFT, Direction::Left, time.t());
		DirectionInput(game.moveDown, game.moveDownTimestamp, SDL_SCANCODE_RIGHT, Direction::Right, time.t());
		DirectionInput(game.moveDown, game.moveDownTimestamp, SDL_SCANCODE_UP, Direction::Up, time.t());
		DirectionInput(game.moveDown, game.moveDownTimestamp, SDL_SCANCODE_DOWN, Direction::Down, time.t());

		Direction direction = Direction::Invalid;
		float latestTime = 0.0f;
		for (Direction dir = Direction::Left; dir < Direction::Count; ++dir)
		{
			if (game.moveDown[dir] && game.moveDownTimestamp[dir] > latestTime)
			{
				direction = dir;
				latestTime = game.moveDownTimestamp[dir];
			}
		}

		game.moveInput = DirectionVelocity(direction);

		if (input::GetKeyDown(SDL_SCANCODE_Z))
		{
			if (Length(game.dashVelocity) < 0.1f)
			{
				game.requestDash = true;
			}
		}

		//Vec2 velocity = moveInput * time.dt() * 10.0f;
		//game.playerPos = game.playerPos + velocity;

		if (direction != Direction::Invalid)
			game.playerFacing = direction;

		//if (game.playerPos.x > game.camera.position.x + 10) game.camera.position.x = game.playerPos.x - 10;
		//if (game.playerPos.x < game.camera.position.x + 5) game.camera.position.x = game.playerPos.x - 5;
		//if (game.playerPos.y > game.camera.position.y + 6) game.camera.position.y = game.playerPos.y - 6;
		//if (game.playerPos.y < game.camera.position.y + 3) game.camera.position.y = game.playerPos.y - 3;
	}

	void FixedUpdate(GameState& game, const GameTime& time)
	{
		if (game.requestDash)
		{
			game.requestDash = false;
			if (Length(game.dashVelocity) < 2.0f)
			{
				game.dashVelocity = DirectionVelocity(game.playerFacing) * 25.0f;
			}
		}

		if (Length(game.dashVelocity) > 0.0f)
		{
			Vec2 decay = -game.dashVelocity * game.dashDecay;
			Vec2 newDashVelocity = game.dashVelocity + decay;
			if (Dot(game.dashVelocity, newDashVelocity) <= 0.0f)
			{
				game.dashVelocity = Vec2{};
			}
			else
			{
				game.dashVelocity = newDashVelocity;
			}
		}

		Vec2 velocity = (game.moveInput * 10.0f + game.dashVelocity) * time.dt();

		game.playerPos = game.playerPos + velocity;

		if (game.playerPos.x > game.camera.position.x + 10) game.camera.position.x = game.playerPos.x - 10;
		if (game.playerPos.x < game.camera.position.x + 5) game.camera.position.x = game.playerPos.x - 5;
		if (game.playerPos.y > game.camera.position.y + 6) game.camera.position.y = game.playerPos.y - 6;
		if (game.playerPos.y < game.camera.position.y + 3) game.camera.position.y = game.playerPos.y - 3;
	}

	void Render(const DrawContext& ctx, const GameState& game, const GameTime& time)
	{
		map::DrawLayers(ctx, game.map, game.camera, game.sprites, std::array{ StrId("Background") });

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
			flags::Set(flip, SpriteFlipFlags::FlipX, true);

		Vec2 screenPos = camera::WorldToScreen(game.camera, game.playerPos);
		sprite::Draw(ctx, game.sprites, spriteId, screenPos.x, screenPos.y, 0.0f, flip, 0.5f, 0.5f);
		SDL_SetRenderDrawColor(ctx.renderer, 255, 0, 0, 255);
		SDL_RenderDrawPointF(ctx.renderer, screenPos.x, screenPos.y);

		map::DrawLayers(ctx, game.map, game.camera, game.sprites, std::array{ StrId("Foreground") });
	}
}