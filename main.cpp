
#include "types.h"
#include <SDL2/SDL.h>
#include "sokol_time.h"
#include "sprites.h"

#include <box2d/box2d.h>

struct Input
{
	struct Keys
	{
		bool currDown[SDL_NUM_SCANCODES];
		bool prevDown[SDL_NUM_SCANCODES];
	} keys;
};

namespace input
{
	void BeginNewFrame(Input& input);
	void KeyDownEvent(Input& input, SDL_Scancode key, bool isPressed);
	bool GetKey(const Input& input, SDL_Scancode key);
	bool GetKeyDown(const Input& input, SDL_Scancode key);
	bool GetKeyUp(const Input& input, SDL_Scancode key);
}

struct Vec2
{
	float x = 0.0f, y = 0.0f;
};

struct Camera
{
	Vec2 position;
	Vec2 extents;
	float pixelsToUnit = 16.0f;
};

namespace camera
{
	Vec2 WorldToScreen(const Camera& camera, Vec2 world);
}

int main(int argc, char* argv[])
{
	SDL_Window* window = SDL_CreateWindow("Lazer Punk", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, 0);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_RenderSetLogicalSize(renderer, 320, 180);

	Camera gameCamera;

	{
		int lsX, lsY;
		SDL_RenderGetLogicalSize(renderer, &lsX, &lsY);
		gameCamera.extents.x = static_cast<float>(lsX);
		gameCamera.extents.y = static_cast<float>(lsY);
	}

	SpriteSheet sheet = sprite_sheet::Create(renderer, "assets/spritesheet.png", 16, 16, 1);

	Input gameInput = {};

	stm_setup();

	uint64_t startTime = stm_now();
	uint64_t clock = startTime;
	double fixedTimestep = 1.0 / 60.0;
	double timeSinceLastFixed = 0.0;

	b2Vec2 gravity(0.0f, -10.0f);
	b2World world(gravity);

	b2BodyDef groundBodyDef;
	groundBodyDef.position.Set(0.0f, -6.0f);
	b2Body* groundBody = world.CreateBody(&groundBodyDef);
	b2PolygonShape groundShape;
	groundShape.SetAsBox(50.0f, 1.0f);
	groundBody->CreateFixture(&groundShape, 0.0f);

	b2BodyDef bodyDef;
	bodyDef.type = b2_dynamicBody;
	bodyDef.position = { 0.0f, 4.0f };
	b2Body* body = world.CreateBody(&bodyDef);
	b2PolygonShape dynamicShape;
	dynamicShape.SetAsBox(1.0f, 1.0f);
	b2FixtureDef fixtureDef;
	fixtureDef.shape = &dynamicShape;
	fixtureDef.density = 1.0f;
	fixtureDef.friction = 0.3f;
	body->CreateFixture(&fixtureDef);

	bool isRunning = true;
	while (isRunning)
	{
		input::BeginNewFrame(gameInput);

		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_QUIT:
					isRunning = false;
					break;
				case SDL_KEYDOWN:
				case SDL_KEYUP:
					input::KeyDownEvent(gameInput, event.key.keysym.scancode, event.type == SDL_KEYDOWN);
					break;
				}
			}
		}

		if (input::GetKeyDown(gameInput, SDL_SCANCODE_ESCAPE))
		{
			isRunning = false;
		}

		uint64_t deltaTime = stm_laptime(&clock);
		uint64_t elapsed = stm_diff(stm_now(), startTime);
		double elapsedSec = stm_sec(elapsed);
		float elapsedSecF = static_cast<float>(elapsedSec);
		static bool showSpriteSheet = false;

		if (elapsedSec >= timeSinceLastFixed + fixedTimestep)
		{
			constexpr int32 velocityIter = 6;
			constexpr int32 positionIter = 2;
			world.Step((float)fixedTimestep, velocityIter, positionIter);
			printf("%4.2f %4.2f\n", body->GetPosition().x, body->GetPosition().y);
			timeSinceLastFixed += fixedTimestep;
		}

		if (input::GetKeyDown(gameInput, SDL_SCANCODE_F1))
		{
			showSpriteSheet = !showSpriteSheet;
		}

		SDL_RenderClear(renderer);

		if (showSpriteSheet)
		{
			for (int y = 0; y < sprite_sheet::Rows(sheet); ++y)
			{
				for (int x = 0; x < sprite_sheet::Columns(sheet); ++x)
				{
					int spriteId = sprite_sheet::GetSpriteId(sheet, x, y);
					sprite::Draw(renderer, sheet, spriteId, (float)x * sheet._spriteWidth, (float)y * sheet._spriteHeight);
				}
			}
		}

		b2Vec2 position = body->GetPosition();
		Vec2 worldPos{ position.x, position.y };
		Vec2 screenPos = camera::WorldToScreen(gameCamera, worldPos);;
		
		sprite::Draw(renderer, sheet, 18 + 7 * 49, screenPos.x, screenPos.y);

		SDL_RenderPresent(renderer);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	return 0;
}


namespace input
{
	void BeginNewFrame(Input& input)
	{
		std::memcpy(input.keys.prevDown, input.keys.currDown, sizeof(input.keys.currDown));
	}

	void KeyDownEvent(Input& input, SDL_Scancode key, bool isPressed)
	{
		input.keys.currDown[key] = isPressed;
	}

	bool GetKey(const Input& input, SDL_Scancode key)
	{
		return input.keys.currDown[key];
	}

	bool GetKeyDown(const Input& input, SDL_Scancode key)
	{
		return input.keys.currDown[key] && !input.keys.prevDown[key];
	}

	bool GetKeyUp(const Input& input, SDL_Scancode key)
	{
		return !input.keys.currDown[key] && input.keys.prevDown[key];
	}
}

Vec2 camera::WorldToScreen(const Camera& camera, Vec2 world)
{
	Vec2 result;
	result.x = world.x * camera.pixelsToUnit + camera.extents.x / 2 - camera.position.x * camera.pixelsToUnit;
	result.y = -world.y * camera.pixelsToUnit + camera.extents.y / 2 - camera.position.y * camera.pixelsToUnit;
	return result;
}
