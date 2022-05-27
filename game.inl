#pragma once

enum class FacingDir
{
	Left, Right, Up, Down
};

struct GameState
{
	Camera camera;
	SpriteSheet sprites{};

	Vec2 playerPos{0, 0};
	FacingDir playerFacing = FacingDir::Right;
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

	void Update(GameState& game, const GameTime& time)
	{
		Vec2 moveInput = {};
		if (input::GetKey(SDL_SCANCODE_LEFT)) moveInput.x -= 1.0f;
		if (input::GetKey(SDL_SCANCODE_RIGHT)) moveInput.x += 1.0f;
		if (input::GetKey(SDL_SCANCODE_UP)) moveInput.y -= 1.0f;
		if (input::GetKey(SDL_SCANCODE_DOWN)) moveInput.y += 1.0f;

		if (fabsf(moveInput.y) <= fabsf(moveInput.x))
		{
			moveInput.y = 0.0f;
		}

		Vec2 velocity = moveInput * time.dt() * 10.0f;
		game.playerPos = game.playerPos + velocity;
	}

	void Render(GameState& game)
	{
		Vec2 screenPos = camera::WorldToScreen(game.camera, game.playerPos);
		sprite::Draw(game.sprites, 1043, screenPos.x, screenPos.y);
	}
}