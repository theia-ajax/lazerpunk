
#include "types.h"
#include "gamemap.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "sokol_time.h"
#include "draw.h"
#include "ecs.h"
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

template <typename T>
constexpr T clamp(T v, T min, T max) { return (v < min) ? min : ((v > max) ? max : v); }



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

World world;

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
	GameMap map = map::Load("assets/testmap.tmj");

	DrawContext drawContext{ renderer };

	GameState state = {
		.camera = gameCamera,
		.sprites = sheet,
		.map = map,
	};
	game::Init(state);

	world.RegisterComponent<Transform>();
	world.RegisterComponent<GameInput>();
	world.RegisterComponent<GameInputGather>();
	world.RegisterComponent<Facing>();
	world.RegisterComponent<FacingSprites>();
	world.RegisterComponent<Velocity>();
	world.RegisterComponent<Sprite>();

	auto gatherInputSystem = world.RegisterSystem<GatherInputSystem, GameInputGather, GameInput>();
	auto playerControlSystem = world.RegisterSystem<PlayerControlSystem, GameInput, Transform, Facing, Velocity>();
	auto spriteFacingSystem = world.RegisterSystem<SpriteFacingSystem, Facing, FacingSprites, Sprite>();
	auto moverSystem = world.RegisterSystem<MoverSystem, Transform, Velocity>();

	Entity playerEntity = world.CreateEntity();
	world.AddComponents(playerEntity,
		Transform{ {8, 5} },
		GameInput{},
		GameInputGather{},
		Facing{ Direction::Right },
		Velocity{},
		FacingSprites{ 1043, 1042, 1041 },
		Sprite{});

	//world.AddComponent(playerEntity, Transform{{8, 5}});
	//world.AddComponent(playerEntity, GameInput{});
	//world.AddComponent(playerEntity, GameInputGather{});
	//world.AddComponent(playerEntity, Facing{ Direction::Right });
	//world.AddComponent(playerEntity, Velocity{});
	//world.AddComponent(playerEntity, FacingSprites{ 1043, 1042, 1041 });
	//world.AddComponent(playerEntity, Sprite{});

	Vec2 ssvOffset = {};
	SDL_Point ssvSelection = {};

	constexpr double kTargetFrameTime = 0.0;
	constexpr double kFixedTimeStepSec = 1.0 / 60.0;

	uint64_t startTime = stm_now();
	uint64_t clock = startTime;
	uint64_t lastFrameTime = clock;
	double timeAccumulator = 0.0;
	double secondTimer = 1.0;
	int frameCountThisSecond = 0;
	int fps = 0;

	bool isRunning = true;
	bool showSpriteSheet = false;
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

		secondTimer -= deltaSec;
		if (secondTimer <= 0.0)
		{
			secondTimer += 1.0;
			fps = frameCountThisSecond;
			frameCountThisSecond = 0;

			char buffer[32];
			snprintf(buffer, 32, "Lazer Punk (FPS:%d)", fps);
			SDL_SetWindowTitle(window, buffer);
		}

		GameTime gameTime(elapsedSec, deltaSec);
		game::Update(state, gameTime);

		gatherInputSystem->Update(gameTime);
		playerControlSystem->Update(gameTime);
		spriteFacingSystem->Update();
		moverSystem->Update(gameTime);

		timeAccumulator += deltaSec;
		while (timeAccumulator >= kFixedTimeStepSec)
		{
			game::FixedUpdate(state, GameTime{ elapsedSec, kFixedTimeStepSec });
			timeAccumulator -= kFixedTimeStepSec;
		}

		if (showSpriteSheet)
		{
			if (input::GetKeyRepeat(SDL_SCANCODE_H)) ssvSelection.x--;
			if (input::GetKeyRepeat(SDL_SCANCODE_L)) ssvSelection.x++;
			if (input::GetKeyRepeat(SDL_SCANCODE_K)) ssvSelection.y--;
			if (input::GetKeyRepeat(SDL_SCANCODE_J)) ssvSelection.y++;

			ssvSelection.x = clamp(ssvSelection.x, 0, sprite_sheet::Columns(sheet) - 1);
			ssvSelection.y = clamp(ssvSelection.y, 0, sprite_sheet::Rows(sheet) - 1);

			SDL_Point viewBase{ static_cast<int>(-ssvOffset.x / sheet.spriteWidth), static_cast<int>(-ssvOffset.y / sheet.spriteHeight) };

			int maxX = (canvasX - 64) / 16 - 1;
			int maxY = canvasY / 16 - 1;
			if (ssvSelection.x - viewBase.x > maxX) viewBase.x = ssvSelection.x - maxX;
			if (ssvSelection.x - viewBase.x < 0) viewBase.x = ssvSelection.x;
			if (ssvSelection.y - viewBase.y > maxY) viewBase.y = ssvSelection.y - maxY;
			if (ssvSelection.y - viewBase.y < 0) viewBase.y = ssvSelection.y;

			ssvOffset.x = static_cast<float>(-viewBase.x * sheet.spriteWidth);
			ssvOffset.y = static_cast<float>(-viewBase.y * sheet.spriteHeight);
		}

		draw::Clear(drawContext);

		//game::Render(drawContext, state, gameTime);

		{
			const auto& playerTx = world.GetComponent<Transform>(playerEntity);
			const auto& sprite = world.GetComponent<Sprite>(playerEntity);
			Vec2 screenPos = camera::WorldToScreen(state.camera, playerTx.position);
			draw::Point(drawContext, screenPos, { 255, 0, 0, 255 });
			draw::Sprite(drawContext, sheet, sprite.spriteId, screenPos, playerTx.rotation, sprite.flipFlags, vec2::Half);
		}


		if (showSpriteSheet)
		{
			draw::Clear(drawContext);

			for (int y = 0; y < sprite_sheet::Rows(sheet); ++y)
			{
				for (int x = 0; x < sprite_sheet::Columns(sheet); ++x)
				{
					int spriteId = sprite_sheet::GetSpriteId(sheet, x, y);
					Vec2 screenPosition = vec2::Create(x, y) * sheet.spriteExtents + ssvOffset;
					draw::Sprite(drawContext, sheet, spriteId, screenPosition, 0.0f, SpriteFlipFlags::None);
				}
			}

			Vec2 ssvSelectionF{ static_cast<float>(ssvSelection.x), static_cast<float>(ssvSelection.y) };
			SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
			SDL_Rect selRect{ static_cast<int>(ssvSelection.x * sheet.spriteWidth + ssvOffset.x), static_cast<int>(ssvSelection.y * sheet.spriteHeight + ssvOffset.y), sheet.spriteWidth, sheet.spriteHeight };
			SDL_RenderDrawRect(renderer, &selRect);

			DrawRect selDrawRect{ ssvSelectionF * sheet.spriteExtents + ssvOffset, sheet.spriteExtents };
			draw::Rect(drawContext, selDrawRect, Color{ 255, 255, 0, 255 });

			SDL_Rect panelRect{ canvasX - 64, 0, 64, canvasY };
			DrawRect panelDrawRect = draw_rect::Create(panelRect);
			draw::RectFill(drawContext, panelDrawRect, Color{ 32, 32, 32, 255 });
			draw::Rect(drawContext, panelDrawRect, Color{ 192, 192, 192, 255 });

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

		++frameCountThisSecond;

		if (kTargetFrameTime > 0)
		{
			while (stm_sec(stm_diff(stm_now(), lastFrameTime)) < kTargetFrameTime) {}
			lastFrameTime = stm_now();
		}
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