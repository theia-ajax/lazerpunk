#define _CRT_SECURE_NO_WARNINGS

#include <array>
#include <format>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "components.h"
#include "debug.h"
#include "draw.h"
#include "ecs.h"
#include "input.h"
#include "random.h"
#include "sokol_time.h"
#include "systems.h"
#include "types.h"

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
	TTF_Font* debugFont = TTF_OpenFont("assets/PressStart2P-Regular.ttf", 16);
	TTF_Font* ssvFont = TTF_OpenFont("assets/PressStart2P-Regular.ttf", 8);

	random::GameRandGen rng(stm_now() | (stm_now() << 24));

	SDL_Window* window = SDL_CreateWindow("Lazer Punk", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, 0);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	constexpr int canvasX = 256, canvasY = 144;
	SDL_RenderSetLogicalSize(renderer, canvasX, canvasY);
	Vec2 viewExtents{ static_cast<float>(canvasX), static_cast<float>(canvasY) };

	InitDevConsole(debug::DevConsoleConfig{ canvasX * 4, canvasY * 4, debugFont, renderer });
	debug::DevConsoleAddCommand("sreport", [] {PrintStringReport(StrId::QueryStringReport()); return 0; });

	SpriteSheet sheet = sprite_sheet::Import("assets/spritesheet.tsj", renderer);
	//SpriteSheet sheet = sprite_sheet::Create(renderer, "assets/spritesheet.png", 16, 16, 1);;
	GameMapHandle map = map::Load("assets/testmap.tmj");

	DrawContext drawContext{ renderer, sheet, debugFont, {canvasX, canvasY} };
	SpriteSheetViewContext ssv{ sheet, ssvFont, canvasX, canvasY };
	debug::DevConsoleAddCommand("ssv", [&ssv] { ssv.visible = !ssv.visible; return 0; });

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
		EnemyTag,
		PhysicsBody,
		Collider::Box, Collider::Circle,
		DebugMarker,
		PhysicsNudge>();

	auto expirationSystem = EntityExpirationSystem::Register(world);
	auto viewSystem = ViewSystem::Register(world);
	auto gatherInputSystem = GatherInputSystem::Register(world);
	auto playerControlSystem = PlayerControlSystem::Register(world);
	auto playerShootSystem = PlayerShootControlSystem::Register(world);
	auto spriteFacingSystem = SpriteFacingSystem::Register(world);
	auto spriteRenderSystem = SpriteRenderSystem::Register(world);
	auto gameMapRenderSystem = GameMapRenderSystem::Register(world);
	auto cameraControlSystem = GameCameraControlSystem::Register(world);
	auto enemyFollowSystem = EnemyFollowTargetSystem::Register(world);
	auto nudgeSystem = PhysicsNudgeSystem::Register(world, SystemFlags::Monitor);
	auto physicsSystem = PhysicsSystem::Register(world);
	auto debugMarkerSystem = ColliderDebugDrawSystem::Register(world);
	auto physicsBodyVelocitySystem = PhysicsBodyVelocitySystem::Register(world);

	auto [cameraEntity, mapEntity, playerEntity] = world.CreateEntities<3>();

	physicsSystem->SetMap(map);
	enemyFollowSystem->targetEntity = playerEntity;

	debug::DevConsoleAddCommand("reload", [&]
		{
			map::Reload(map);
			physicsSystem->SetMap(map);
			return 0;
		});

	world.AddComponents(cameraEntity,
		Transform{},
		CameraView{ viewExtents },
		GameCameraControl{ map, playerEntity, {{-2, -0.75f}, {2, 0.75f}} });

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
		FacingSprites{ 13, 11, 12 },
		SpriteRender{ 10, SpriteFlipFlags::None, vec2::Half },
		Collider::Box{ vec2::Zero, vec2::One * 0.45f },
		PhysicsBody{},
		DebugMarker{});

	Entity enemyPrefab = world.CreateEntity();
	world.AddComponents(enemyPrefab,
		Prefab{},
		Transform{},
		Velocity{},
		SpriteRender{ 26, SpriteFlipFlags::None, vec2::Half },
		EnemyTag{},
		PhysicsBody{},
		PhysicsNudge{ 0.6f, 0.33f, 5.0f },
		Collider::Box{ vec2::Zero, vec2::One * 0.45f });

	uint64_t start = stm_now();
	constexpr int ENEMY_COUNT = 25;
#if 0
	for (int i = 0; i < ENEMY_COUNT; ++i)
	{
		Entity enemy = world.CloneEntity(enemyPrefab);
		world.RemoveComponent<Prefab>(enemy);
		Vec2 position;
		do
		{
			position = { rng.RangeF(1.0f, 31.0f), rng.RangeF(1.0f, 15.0f) };
		} while (physicsSystem->MapSolid(Bounds2D::FromCenter(position, vec2::Half)));
		world.GetComponent<Transform>(enemy).position = position;
	}
#else
	for (auto enemyEntities = world.CreateEntities<ENEMY_COUNT>(); Entity enemy : enemyEntities)
	{
		Vec2 position;
		do
		{
			position = { rng.RangeF(1.0f, 31.0f), rng.RangeF(1.0f, 15.0f) };
		} while (physicsSystem->MapSolid(Bounds2D::FromCenter(position, vec2::Half)));

		world.AddComponents(enemy,
			Transform{ position },
			Velocity{},
			SpriteRender{ 26, SpriteFlipFlags::None, vec2::Half },
			EnemyTag{},
			PhysicsBody{},
			PhysicsNudge{ 0.6f, 0.33f, 5.0f },
			Collider::Box{ vec2::Zero, vec2::One * 0.45f });
	}
#endif
	uint64_t took = stm_diff(stm_now(), start);
	debug::Log("Cloning {} enemies took {}ms", ENEMY_COUNT, stm_ms(took));

	cameraControlSystem->SnapFocusToFollow(cameraEntity);

	constexpr double kTargetFrameTime = 1.0 / 60;
	constexpr double kFixedTimeStepSec = 1.0 / 60.0;

	uint64_t startTime = stm_now();
	uint64_t clock = startTime;
	uint64_t lastFrameTime = clock;
	double timeAccumulator = 0.0;
	double secondTimer = 1.0;
	int frameCountThisSecond = 0;
	int fps = 0;
	uint64_t frameTicks = 0;

	bool isRunning = true;
	bool showColliders = false;
	bool showDebugWatch = _DEBUG;
	debug::DevConsoleAddCommand("colliders", [&showColliders] { showColliders = !showColliders; return 0; });
	debug::DevConsoleAddCommand("watch", [&showDebugWatch](bool show) { showDebugWatch = show; return show; });
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
					debug::DevConsoleKeyInput(event.key.keysym.scancode, event.type == SDL_KEYDOWN, event.key.repeat);
					break;
				case SDL_TEXTINPUT:
					debug::DevConsoleTextInput(event.text.text);
					break;
				case SDL_TEXTEDITING:
					debug::DevConsoleTextEdit(event.edit.text, event.edit.start, event.edit.length);
					break;
				}
			}
		}
		if (input::GetKeyDown(SDL_SCANCODE_ESCAPE))
		{
			isRunning = false;
		}

		if (input::GetKeyDown(SDL_SCANCODE_GRAVE))
			debug::ToggleDevConsole();

		uint64_t deltaTime = stm_laptime(&clock);
		uint64_t elapsed = stm_diff(stm_now(), startTime);
		double elapsedSec = stm_sec(elapsed);
		double deltaSec = math::min(stm_sec(deltaTime), 1.0);

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

		debug::Watch("FPS: {:d}", fps);
		debug::Watch("Frame MS: {:.3f}", stm_ms(frameTicks));
		debug::Watch("Time: {:.2f}", elapsedSec);

		GameTime gameTime(elapsedSec, deltaSec);

		expirationSystem->Update(gameTime);
		gatherInputSystem->Update(gameTime);
		playerControlSystem->Update(gameTime);
		playerShootSystem->Update(gameTime);
		enemyFollowSystem->Update(gameTime);
		spriteFacingSystem->Update();
		physicsBodyVelocitySystem->Update(gameTime);
		nudgeSystem->Update(gameTime);
		physicsSystem->Update(gameTime);
		cameraControlSystem->Update(gameTime);
		viewSystem->Update(gameTime);

		SpriteSheetViewControl(ssv);

		draw::Clear(drawContext);
		gameMapRenderSystem->RenderLayers(drawContext, std::array{ StrId("Background") });
		spriteRenderSystem->Render(drawContext);
		if (showColliders) debugMarkerSystem->DrawMarkers(drawContext);

		SpriteSheetViewRender(drawContext, ssv);

		if (showDebugWatch)
			debug::DrawWatch(drawContext);
		debug::DrawConsole(drawContext, gameTime.dt());

		SDL_RenderPresent(renderer);

		++frameCountThisSecond;

		frameTicks = stm_now() - clock;

		if constexpr (kTargetFrameTime > 0)
		{
			while (stm_sec(stm_diff(stm_now(), lastFrameTime)) < kTargetFrameTime) {}
			lastFrameTime = stm_now();
		}
	}

	debug::LogWriteToFile("log.txt");

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
	debug::Log("String Report:");
	debug::Log("    Block Memory: {:d}", report.blockSize * report.blockCapacity);
	debug::Log("    Block Usage: {:d}", report.blockSize * report.blockCount);
	debug::Log("    Entries: {:d} / {:d}", report.entryCount, report.entryCapacity);
	debug::Log("Stored strings:");
	for (const auto& s : report.strings)
	{
		debug::Log("    {}", s.c_str());
	}
}