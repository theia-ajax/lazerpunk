
#include "types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "sokol_time.h"
#include "input.h"
#include "draw.h"
#include "ecs.h"
#include "components.h"
#include "systems.h"
#include "random.h"
#include <array>

World world;

struct SpriteSheetViewContext
{
	SpriteSheet& sheet;
	TTF_Font* debugFont{};
	int canvasX{}, canvasY{};
	bool visible{};
	Vec2 offset{};
	SDL_Point selection{};
};

void SpriteSheetViewControl(SpriteSheetViewContext& ssv);
void SpriteSheetViewRender(const DrawContext& ctx, const SpriteSheetViewContext& ssv);
void PrintStringReport(const StringReport& report);

int main(int argc, char* argv[])
{
	stm_setup();
	TTF_Init();
	TTF_Font* debugFont = TTF_OpenFont("assets/PressStart2P-Regular.ttf", 8);

	random::GameRandGen rng(stm_now() | (stm_now() << 24));

	SDL_Window* window = SDL_CreateWindow("Lazer Punk", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, 0);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	SDL_RenderSetLogicalSize(renderer, 256, 144);

	int canvasX, canvasY;
	SDL_RenderGetLogicalSize(renderer, &canvasX, &canvasY);
	Vec2 viewExtents{ static_cast<float>(canvasX), static_cast<float>(canvasY) };

	SpriteSheet sheet = sprite_sheet::Create(renderer, "assets/spritesheet.png", 16, 16, 1);;
	GameMapHandle map = map::Load("assets/testmap.tmj");

	DrawContext drawContext{ renderer, sheet };
	SpriteSheetViewContext ssv{ sheet, debugFont, canvasX, canvasY };

	world.RegisterComponents<
		Expiration,
		Transform, Velocity,
		GameInputGather, GameInput,
		PlayerControl,
		PlayerShootControl,
		Facing,
		FacingSprites,
		CameraView,
		SpriteRender,
		GameMapRender,
		GameCameraControl,
		EnemyTag>();

	auto expirationSystem = world.RegisterSystem<EntityExpirationSystem, Expiration>();
	auto viewSystem = world.RegisterSystem<ViewSystem, Transform, CameraView>();
	auto gatherInputSystem = world.RegisterSystem<GatherInputSystem, GameInputGather, GameInput>();
	auto playerControlSystem = world.RegisterSystem<PlayerControlSystem, GameInput, Transform, Facing, Velocity, PlayerControl>();
	auto playerShootSystem = world.RegisterSystem<PlayerShootControlSystem, GameInput, Transform, Facing, Velocity, PlayerShootControl>();
	auto spriteFacingSystem = world.RegisterSystem<SpriteFacingSystem, Facing, FacingSprites, SpriteRender>();
	auto moverSystem = world.RegisterSystem<MoverSystem, Transform, Velocity>();
	auto spriteRenderSystem = world.RegisterSystem<SpriteRenderSystem, Transform, SpriteRender>();
	auto gameMapRenderSystem = world.RegisterSystem<GameMapRenderSystem, Transform, GameMapRender>();
	auto cameraControlSystem = world.RegisterSystem<GameCameraControlSystem, Transform, CameraView, GameCameraControl>();
	auto enemyFollowSystem = world.RegisterSystem<EnemyFollowTargetSystem, Transform, Velocity, EnemyTag>();

	auto [cameraEntity, mapEntity, playerEntity] = world.CreateEntities<3>();

	enemyFollowSystem->targetEntity = playerEntity;

	world.AddComponents(cameraEntity,
		Transform{},
		CameraView{ viewExtents },
		GameCameraControl{ map, playerEntity, {{-2, -0.75f}, {2, 0.75f}} });

	static_stack<uint8_t, 4> bytes = {
		1, 2, 3
	};

	world.AddComponents(mapEntity,
		Transform{},
		GameMapRender{ map });

	world.AddComponents(playerEntity,
		Transform{ {8, 5} },
		GameInput{},
		GameInputGather{},
		PlayerControl{},
		PlayerShootControl{ 0.15f },
		Facing{ Direction::Right },
		Velocity{},
		FacingSprites{ 1043, 1042, 1041 },
		SpriteRender{ 1043, SpriteFlipFlags::None, vec2::Half });

	auto enemyEntities = world.CreateEntities<10>();
	for (Entity enemy : enemyEntities)
	{
		world.AddComponents(enemy,
			Transform{ {rng.RangeF(1.0f, 31.0f), rng.RangeF(1.0f, 15.0f)} },
			Velocity{},
			SpriteRender{ 320, SpriteFlipFlags::None, vec2::Half },
			EnemyTag{});
	}

	StringReport report = StrId::QueryStringReport();
	PrintStringReport(report);

	cameraControlSystem->SnapFocusToFollow(cameraEntity);

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
			ssv.visible = !ssv.visible;
		}

		if (input::GetKeyDown(SDL_SCANCODE_F5))
		{
			map::Reload(map);
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

		expirationSystem->Update(gameTime);
		gatherInputSystem->Update(gameTime);
		playerControlSystem->Update(gameTime);
		playerShootSystem->Update(gameTime);
		enemyFollowSystem->Update(gameTime);
		spriteFacingSystem->Update();
		moverSystem->Update(gameTime);
		cameraControlSystem->Update(gameTime);
		viewSystem->Update(gameTime);

		SpriteSheetViewControl(ssv);

		draw::Clear(drawContext);
		gameMapRenderSystem->RenderLayers(drawContext, std::array{ StrId("Background") });
		spriteRenderSystem->Render(drawContext);

		SpriteSheetViewRender(drawContext, ssv);

		SDL_RenderPresent(renderer);

		++frameCountThisSecond;

		printf("Entities: %04u\r", world.GetEntityCount());

		if constexpr (kTargetFrameTime > 0)
		{
			while (stm_sec(stm_diff(stm_now(), lastFrameTime)) < kTargetFrameTime) {}
			lastFrameTime = stm_now();
		}
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	return 0;
}


void SpriteSheetViewControl(SpriteSheetViewContext& ssv)
{
	if (!ssv.visible)
		return;
	if (input::GetKeyRepeat(SDL_SCANCODE_H)) ssv.selection.x--;
	if (input::GetKeyRepeat(SDL_SCANCODE_L)) ssv.selection.x++;
	if (input::GetKeyRepeat(SDL_SCANCODE_K)) ssv.selection.y--;
	if (input::GetKeyRepeat(SDL_SCANCODE_J)) ssv.selection.y++;

	ssv.selection.x = math::clamp(ssv.selection.x, 0, sprite_sheet::Columns(ssv.sheet) - 1);
	ssv.selection.y = math::clamp(ssv.selection.y, 0, sprite_sheet::Rows(ssv.sheet) - 1);

	SDL_Point viewBase{
		static_cast<int>(-ssv.offset.x / ssv.sheet.spriteWidth),
		static_cast<int>(-ssv.offset.y / ssv.sheet.spriteHeight)
	};

	int maxX = (ssv.canvasX - 64) / 16 - 1;
	int maxY = ssv.canvasY / 16 - 1;
	if (ssv.selection.x - viewBase.x > maxX) viewBase.x = ssv.selection.x - maxX;
	if (ssv.selection.x - viewBase.x < 0) viewBase.x = ssv.selection.x;
	if (ssv.selection.y - viewBase.y > maxY) viewBase.y = ssv.selection.y - maxY;
	if (ssv.selection.y - viewBase.y < 0) viewBase.y = ssv.selection.y;

	ssv.offset.x = static_cast<float>(-viewBase.x * ssv.sheet.spriteWidth);
	ssv.offset.y = static_cast<float>(-viewBase.y * ssv.sheet.spriteHeight);
}

void SpriteSheetViewRender(const DrawContext& ctx, const SpriteSheetViewContext& ssv)
{
	if (!ssv.visible)
		return;

	draw::Clear(ctx);

	for (int y = 0; y < sprite_sheet::Rows(ssv.sheet); ++y)
	{
		for (int x = 0; x < sprite_sheet::Columns(ssv.sheet); ++x)
		{
			int spriteId = sprite_sheet::GetSpriteId(ssv.sheet, x, y);
			Vec2 screenPosition = vec2::Create(x, y) * ssv.sheet.spriteExtents + ssv.offset;
			draw::Sprite(ctx, ssv.sheet, spriteId, screenPosition, 0.0f, SpriteFlipFlags::None);
		}
	}

	Vec2 ssvSelectionF{ static_cast<float>(ssv.selection.x), static_cast<float>(ssv.selection.y) };
	SDL_SetRenderDrawColor(ctx.renderer, 255, 255, 0, 255);
	SDL_Rect selRect{
		static_cast<int>(ssv.selection.x * ssv.sheet.spriteWidth + ssv.offset.x),
		static_cast<int>(ssv.selection.y * ssv.sheet.spriteHeight + ssv.offset.y),
		ssv.sheet.spriteWidth,
		ssv.sheet.spriteHeight
	};
	SDL_RenderDrawRect(ctx.renderer, &selRect);

	DrawRect selDrawRect{ ssvSelectionF * ssv.sheet.spriteExtents + ssv.offset, ssv.sheet.spriteExtents };
	draw::Rect(ctx, selDrawRect, Color{ 255, 255, 0, 255 });

	SDL_Rect panelRect{ ssv.canvasX - 64, 0, 64, ssv.canvasY };
	DrawRect panelDrawRect = draw_rect::Create(panelRect);
	draw::RectFill(ctx, panelDrawRect, Color{ 32, 32, 32, 255 });
	draw::Rect(ctx, panelDrawRect, Color{ 192, 192, 192, 255 });

	char buffer[32];
	int selSpriteId = ssv.selection.y * sprite_sheet::Columns(ssv.sheet) + ssv.selection.x;
	snprintf(buffer, 32, "ID:%d\nX :%d\nY :%d", selSpriteId, ssv.selection.x, ssv.selection.y);
	SDL_Surface* textSurface = TTF_RenderText_Blended_Wrapped(ssv.debugFont, buffer, SDL_Color{ 192, 192, 192, 255 }, 64);
	SDL_Texture* textTexture = SDL_CreateTextureFromSurface(ctx.renderer, textSurface);
	SDL_Rect textRect{ panelRect.x + 2, panelRect.y + 2, textSurface->w, textSurface->h };
	SDL_RenderCopy(ctx.renderer, textTexture, nullptr, &textRect);
	SDL_DestroyTexture(textTexture);
	SDL_FreeSurface(textSurface);
}

namespace internal
{
	void PrintAssert(const char* function, int lineNum, const char* exprStr)
	{
		printf("ASSERT FAILED (%s) in %s:%d\n", exprStr, function, lineNum);
	}
}

void PrintStringReport(const StringReport& report)
{

	printf("String Report:\n");
	printf("\tBlock Memory: %d\n", report.blockSize * report.blockCapacity);
	printf("\tBlock Usage: %d\n", report.blockSize * report.blockCount);
	printf("\tEntries: %d / %d\n", report.entryCount, report.entryCapacity);
	printf("\tStored strings:\n");
	for (const auto& s : report.strings)
	{
		printf("\t\t%s\n", s.c_str());
	}
}