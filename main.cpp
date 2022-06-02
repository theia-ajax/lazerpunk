
#include "types.h"
#include "gamemap.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "sokol_time.h"
#include "sprites.h"
#include <array>

namespace input
{
	void BeginNewFrame();
	void KeyDownEvent(SDL_Scancode key, bool isPressed, bool isRepeat);
	bool GetKey(SDL_Scancode key);
	bool GetKeyDown(SDL_Scancode key);
	bool GetKeyUp(SDL_Scancode key);
	bool GetKeyRepeat(SDL_Scancode key);
}

struct Point2I
{
	int x = 0, y = 0;
};

inline int clamp(int v, int min, int max) { return (v < min) ? min : ((v > max) ? max : v); }

constexpr uint64_t TicksInSecond(double sec) {
	return (uint64_t)(sec * 1000000000.0);
}

struct GameTime
{
	GameTime(double elapsed, double delta) : elapsedSec(elapsed), deltaSec(delta) {}
	float t() const { return static_cast<float>(elapsedSec); }
	float dt() const { return static_cast<float>(deltaSec); }
private:
	const double elapsedSec;
	const double deltaSec;
};
#include "game.inl"

int main(int argc, char* argv[])
{
	stm_setup();
	TTF_Init();

	TTF_Font* debugFont = TTF_OpenFont("assets/PressStart2P-Regular.ttf", 8);

	SDL_Window* window = SDL_CreateWindow("Lazer Punk", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, 0);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	SDL_RenderSetLogicalSize(renderer, 256, 144);
	Camera gameCamera;
	int canvasX, canvasY;
	SDL_RenderGetLogicalSize(renderer, &canvasX, &canvasY);
	gameCamera.extents.x = static_cast<float>(canvasX);
	gameCamera.extents.y = static_cast<float>(canvasY);

	SpriteSheet sheet = sprite_sheet::Create(renderer, "assets/spritesheet.png", 16, 16, 1);;

	uint64_t startTime = stm_now();
	uint64_t clock = startTime;
	constexpr double kFixedTimeStepSec = 1.0 / 60.0;
	constexpr uint64_t kFixedTimeStepTicks = TicksInSecond(kFixedTimeStepSec);
	uint64_t lastFixedUpdate = startTime;

	GameMap map = map::Load("assets/testmap.tmj");

	DrawContext drawContext{ renderer };

	GameState state = {
		.camera = gameCamera,
		.sprites = sheet,
		.map = map,
	};
	game::Init(state);

	bool isRunning = true;
	bool showSpriteSheet = false;
	Vec2 ssvOffset = {};
	Point2I ssvSelection = {};
	while (isRunning)
	{
		input::BeginNewFrame();

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
					input::KeyDownEvent(event.key.keysym.scancode, event.type == SDL_KEYDOWN, event.key.repeat);
					break;
				}
			}
		}
		if (input::GetKeyDown(SDL_SCANCODE_ESCAPE))
		{
			isRunning = false;
		}

		if (input::GetKeyDown(SDL_SCANCODE_F1))
		{
			showSpriteSheet = !showSpriteSheet;
		}

		if (input::GetKeyDown(SDL_SCANCODE_F5))
		{
			map::Reload(state.map);
		}

		uint64_t deltaTime = stm_laptime(&clock);
		uint64_t elapsed = stm_diff(stm_now(), startTime);
		double elapsedSec = stm_sec(elapsed);
		double deltaSec = stm_sec(deltaTime);
		double elapsedMs = stm_ms(deltaTime);

		GameTime gameTime(elapsedSec, deltaSec);
		game::Update(state, gameTime);

		if (showSpriteSheet)
		{
			if (input::GetKeyRepeat(SDL_SCANCODE_H)) ssvSelection.x--;
			if (input::GetKeyRepeat(SDL_SCANCODE_L)) ssvSelection.x++;
			if (input::GetKeyRepeat(SDL_SCANCODE_K)) ssvSelection.y--;
			if (input::GetKeyRepeat(SDL_SCANCODE_J)) ssvSelection.y++;

			ssvSelection.x = clamp(ssvSelection.x, 0, sprite_sheet::Columns(sheet) - 1);
			ssvSelection.y = clamp(ssvSelection.y, 0, sprite_sheet::Rows(sheet) - 1);

			Point2I viewBase{ static_cast<int>(-ssvOffset.x / sheet._spriteWidth), static_cast<int>(-ssvOffset.y / sheet._spriteHeight) };

			int maxX = (canvasX - 64) / 16 - 1;
			int maxY = canvasY / 16 - 1;
			if (ssvSelection.x - viewBase.x > maxX) viewBase.x = ssvSelection.x - maxX;
			if (ssvSelection.x - viewBase.x < 0) viewBase.x = ssvSelection.x;
			if (ssvSelection.y - viewBase.y > maxY) viewBase.y = ssvSelection.y - maxY;
			if (ssvSelection.y - viewBase.y < 0) viewBase.y = ssvSelection.y;

			ssvOffset.x = static_cast<float>(-viewBase.x * sheet._spriteWidth);
			ssvOffset.y = static_cast<float>(-viewBase.y * sheet._spriteHeight);
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

		game::Render(drawContext, state, gameTime);

		if (showSpriteSheet)
		{
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);

			for (int y = 0; y < sprite_sheet::Rows(sheet); ++y)
			{
				for (int x = 0; x < sprite_sheet::Columns(sheet); ++x)
				{
					int spriteId = sprite_sheet::GetSpriteId(sheet, x, y);
					sprite::Draw(drawContext, sheet, spriteId, (float)x * sheet._spriteWidth + ssvOffset.x, (float)y * sheet._spriteHeight + ssvOffset.y, 0.0, SpriteFlipFlags::None);
				}
			}

			SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
			SDL_Rect selRect{ static_cast<int>(ssvSelection.x * sheet._spriteWidth + ssvOffset.x), static_cast<int>(ssvSelection.y * sheet._spriteHeight + ssvOffset.y), sheet._spriteWidth, sheet._spriteHeight };
			SDL_RenderDrawRect(renderer, &selRect);

			SDL_Rect panelRect{ canvasX - 64, 0, 64, canvasY };
			SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
			SDL_RenderFillRect(renderer, &panelRect);
			SDL_SetRenderDrawColor(renderer, 192, 192, 192, 255);
			SDL_RenderDrawRect(renderer, &panelRect);

			char buffer[32];
			int selSpriteId = ssvSelection.y * sprite_sheet::Columns(sheet) + ssvSelection.x;
			snprintf(buffer, 32, "ID:%d\nX :%d\nY :%d", selSpriteId, ssvSelection.x, ssvSelection.y);
			SDL_Surface* textSurface = TTF_RenderText_Blended_Wrapped(debugFont, buffer, SDL_Color{ 192, 192, 192, 255 }, 64);
			SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
			SDL_Rect textRect{ panelRect.x + 2, panelRect.y + 2, textSurface->w, textSurface->h };
			SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
			SDL_DestroyTexture(textTexture);
			SDL_FreeSurface(textSurface);
		}

		SDL_RenderPresent(renderer);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	return 0;
}

namespace input
{
	struct Input
	{
		struct KeyState
		{
			bool isDown;
			bool isRepeat;
		};

		struct Keys
		{
			KeyState currDown[SDL_NUM_SCANCODES];
			KeyState prevDown[SDL_NUM_SCANCODES];
		} keys;
	};

	namespace
	{
		Input input{};
	}


	void BeginNewFrame()
	{
		std::memcpy(input.keys.prevDown, input.keys.currDown, sizeof(input.keys.currDown));
	}

	void KeyDownEvent(SDL_Scancode key, bool isPressed, bool isRepeat)
	{
		input.keys.currDown[key] = { isPressed, isRepeat };
	}

	bool GetKey(SDL_Scancode key)
	{
		return input.keys.currDown[key].isDown;
	}

	bool GetKeyDown(SDL_Scancode key)
	{
		return input.keys.currDown[key].isDown && !input.keys.prevDown[key].isDown && !input.keys.currDown[key].isRepeat;
	}

	bool GetKeyUp(SDL_Scancode key)
	{
		return !input.keys.currDown[key].isDown && input.keys.prevDown[key].isDown;
	}

	bool GetKeyRepeat(SDL_Scancode key)
	{
		return input.keys.currDown[key].isDown && input.keys.currDown[key].isRepeat || GetKeyDown(key);
	}
}

namespace internal
{
	void PrintAssert(const char* function, int lineNum, const char* exprStr)
	{
		printf("ASSERT FAILED (%s) in %s:%d\n", exprStr, function, lineNum);
	}
}