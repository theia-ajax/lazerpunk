#define _CRT_SECURE_NO_WARNINGS

#include <array>
#include <format>
#include <numbers>
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

struct TestColor
{
	Color color = { 255, 255, 255, 255 };
};

struct TestSize
{
	float size = 1.0f;
};

struct TestIndex
{
	int index{};
	float time{};
};


template<>
struct std::formatter<TestIndex> : std::formatter<std::string>
{
	auto format(TestIndex ti, format_context& ctx) const
	{
		return formatter<string>::format(std::format("{}", ti.index), ctx);
	}
};

template <typename Iter>
void LogIterator(Iter first, Iter last, const std::string& typeLabel, const std::string& label)
{
	ptrdiff_t index = 0;
	ptrdiff_t count = std::distance(first, last);

	std::string elementStrs;
	elementStrs.reserve(256);

	std::for_each(first, last, [&index, count, &elementStrs](const auto& value)
		{

			std::unwrap_ref_decay_t<decltype(value)> v = value;
			elementStrs += std::format("{}{}", v, (index < count - 1) ? ", " : " ");
			++index;
		});

	using T = typename std::iterator_traits<Iter>::value_type;

	debug::Log("{} {}<{}> [{}] : {}", label, typeLabel, typeid(T).name(), count, elementStrs);
}


struct TestSystem : System<TestSystem, TestIndex, Transform, TestSize, TestColor>
{
	float t = 0;
	Query<TestIndex, TestSize>* query{};
	size_t lastSize = 0;

	void OnRegistered() override
	{
		query = GetWorld().CreateQuery<TestIndex, TestSize>({}, [](World& world, Entity a, Entity b)
			{
				auto& idxA = world.GetComponent<TestIndex>(a);
				auto& idxB = world.GetComponent<TestIndex>(b);
				return idxA.index < idxB.index;
			});
	}

	void Update(const GameTime& time)
	{
		if (query->GetEntities().size() > lastSize)
		{
			query->LogComponentList<TestIndex>([](const TestIndex& ti)
				{
					return std::to_string(ti.index);
				});
		}
		lastSize = query->GetEntities().size();

		for (auto& entities = GetEntities(); Entity entity : entities)
		{
			auto [index, transform, size, color] = GetArchetype(entity);


			float indexRatio = static_cast<float>(index.index) / entities.size();
			index.time += time.dt() * indexRatio * math::Phi;
			float radians = math::E * math::Phi * index.index + index.time + time.t();
			float radius = 0.5f + indexRatio * math::Phi * 4.0f;

			Vec2 targetPosition = Vec2{ 8, 5 } + Vec2{ std::cos(radians), std::sin(radians) } * radius;

			transform.position = vec2::Damp(transform.position, targetPosition, 5.0f, time.dt());
		}

		if (input::GetKeyDown(SDL_SCANCODE_L))
		{
			auto testIndices = query->GetComponentList<TestIndex>();
			bool isSorted = std::ranges::is_sorted(testIndices, [](const TestIndex& a, const TestIndex& b) { return a.index < b.index;  });
			debug::Log("query sorted: {}", isSorted);
			auto last = std::ranges::upper_bound(testIndices, TestIndex{ 4 }, [this](const TestIndex& a, const TestIndex& b) { return a.index < b.index; });
			auto first = testIndices.begin();
			int32_t count = static_cast<int32_t>(std::distance(testIndices.begin(), last));
			intptr_t firstIndex = std::distance(testIndices.begin(), first);
			intptr_t lastIndex = std::distance(testIndices.begin(), last);

			for (auto iter = first; iter != last; ++iter)
			{
				int32_t idx = static_cast<int32_t>(std::distance(first, iter));
				Entity spawned = query->GetEntityAtIndex(idx);
				GetWorld().AddComponent<Expiration>(spawned, Expiration{});
			}
		}
	}

	void Render(const DrawContext& ctx, const Camera& activeCamera)
	{
		for (auto& entities = GetEntities(); Entity entity : entities)
		{
			auto [index, transform, size, color] = GetArchetype(entity);

			Vec2 screenPos = camera::WorldToScreen(activeCamera, transform.position);
			Vec2 screenSize = vec2::One * size.size;

			DrawRect r{ screenPos - screenSize / 2, screenSize };
			draw::RectFill(ctx, r, color.color);
		}
	}
};

struct TestSpawnerSystem : System<TestSpawnerSystem>
{
	Query<TestIndex>* query{};
	float interval = 0.01f;
	float timer = 0.5f;
	static_stack<Entity, 8> spawned{};

	void OnRegistered() override
	{
		query = GetWorld().CreateQuery<TestIndex>({
			[this](Entity entity)
			{
				auto index = GetWorld().GetComponent<TestIndex>(entity);
				spawned[index.index] = entity;
				LogIterator(spawned.begin(), spawned.end(), "static_stack", "Spawned");
				const auto& list = query->GetComponentList<TestIndex>();
				LogIterator(list.begin(), list.end(), "ComponentList", "Query");
			},
			[this](Entity entity)
			{
				auto index = GetWorld().GetComponent<TestIndex>(entity);
				spawned[index.index] = 0;
				LogIterator(spawned.begin(), spawned.end(), "static_stack", "Spawned");
				const auto& list = query->GetComponentList<TestIndex>();
				LogIterator(list.begin(), list.end(), "ComponentList", "Query");
			} }, [](World& world, Entity a, Entity b)
			{
				auto& idxA = world.GetComponent<TestIndex>(a);
				auto& idxB = world.GetComponent<TestIndex>(b);
				return idxA.index < idxB.index;
			});
	}

	void Update(const GameTime& time)
	{
		if (timer > 0.0f) timer -= time.dt();

		if (timer <= 0.0f)
		{
			for (int i = 0; i < 3; ++i)
			{
				auto search = std::ranges::find_if(spawned, [](auto e) { return e == 0; });
				if (search != spawned.end())
				{
					timer += interval;

					Entity entity = GetWorld().CreateEntity();
					*search = entity;

					ptrdiff_t index = std::distance(spawned.begin(), search);

					Color colors[] = {
						{0x18, 0x21, 0xfd, 0xff},
						{0x50, 0x31, 0x97, 0xff},
						{0xbf, 0x53, 0xc9, 0xff},
						{0xe1, 0x7c, 0xb7, 0xff},
						{0xef, 0xaa, 0xa5, 0xff},
						{0xf6, 0xe0, 0xc8, 0xff},
					};

					float ratio = static_cast<float>(index) / spawned.capacity();

					GetWorld().AddComponents(entity, Transform{ {8, 5} }, TestIndex{ static_cast<int>(index) }, TestColor{ colors[index % 6] }, TestSize{ 8 });
				}
			}
		}
		//auto testEntities = world.CreateEntities<36>();
		//{
		//	float sizes[3] = { 2, 3, 4 };
		//	float radii[3] = { 2, 3, 4 };
		//	int index = 0;
		//	for (Entity entity : testEntities)
		//	{
		//		int groupIndex = index * 3 / static_cast<int>(testEntities.size());
		//		world.AddComponents(entity,
		//			Transform{}, TestIndex{ index, radii[groupIndex] }, TestColor{ colors[groupIndex] }, TestSize{ sizes[groupIndex] });
		//		++index;
		//	}
		//}
	}
};



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

	InitDevConsole(debug::DevConsoleConfig{ canvasX * 6, canvasY * 6, debugFont, renderer });
	debug::DevConsoleAddCommand("sreport", [] {PrintStringReport(StrId::QueryStringReport()); return 0; });

	SpriteSheet sheet = sprite_sheet::Import("assets/spritesheet.tsj", renderer);
	//SpriteSheet sheet = sprite_sheet::Create(renderer, "assets/spritesheet.png", 16, 16, 1);;
	GameMapHandle map = map::Load("assets/testmap.tmj");

	DrawContext drawContext{ renderer, sheet, debugFont, {canvasX, canvasY} };
	SpriteSheetViewContext ssv{ sheet, ssvFont, canvasX, canvasY };
	debug::DevConsoleAddCommand("ssv", [&ssv] { ssv.visible = !ssv.visible; return 0; });

	world.RegisterComponents<
		TestColor, TestSize, TestIndex,
		Expiration,
		Transform, Velocity,
		GameInputGather, GameInput,
		PlayerControl, PlayerShootControl,
		Facing, FacingSprites,
		CameraView, GameCameraControl,
		SpriteRender, GameMapRender,
		EnemyTag,
		Spawner, SpawnSource,
		PhysicsBody, Collider::Box, Collider::Circle, PhysicsNudge,
		DebugMarker>();

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
	auto nudgeSystem = PhysicsNudgeSystem::Register(world);
	auto physicsSystem = PhysicsSystem::Register(world);
	auto debugMarkerSystem = ColliderDebugDrawSystem::Register(world);
	auto physicsBodyVelocitySystem = PhysicsBodyVelocitySystem::Register(world);
	auto spawnerSystem = SpawnerSystem::Register(world);
	auto testSystem = TestSystem::Register(world);
	auto testSpawnSystem = TestSpawnerSystem::Register(world);

	auto [cameraEntity, mapEntity, playerEntity] = world.CreateEntities<3>();

	physicsSystem->SetMap(map);
	//enemyFollowSystem->targetEntity = playerEntity;

	debug::DevConsoleAddCommand("reload", [&]
		{
			map::Reload(map);
			physicsSystem->SetMap(map);
			return 0;
		});

	world.AddComponents(cameraEntity,
		Transform{},
		CameraView{ viewExtents },
		GameCameraControl{ map, 0, {{-2, -0.75f}, {2, 0.75f}} });



	//world.AddComponents(mapEntity,
	//	Transform{},
	//	GameMapRender{ map });

	//world.AddComponents(playerEntity,
	//	Transform{ {8, 5} },
	//	GameInput{},
	//	GameInputGather{},
	//	PlayerControl{},
	//	PlayerShootControl{ 0.15f },
	//	Facing{ Direction::Right },
	//	Velocity{},
	//	FacingSprites{ 13, 11, 12 },
	//	SpriteRender{ 10, SpriteFlipFlags::None, vec2::Half },
	//	Collider::Box{ vec2::Zero, vec2::One * 0.45f },
	//	PhysicsBody{},
	//	DebugMarker{});

	//Entity enemyPrefab = world.CreateEntity();
	//world.AddComponents(enemyPrefab,
	//	Prefab{},
	//	Transform{},
	//	Velocity{},
	//	SpriteRender{ 26, SpriteFlipFlags::None, vec2::Half },
	//	EnemyTag{},
	//	PhysicsBody{},
	//	PhysicsNudge{ 0.6f, 0.33f, 5.0f },
	//	Collider::Box{ vec2::Zero, vec2::One * 0.45f });

	auto findSafeSpot = [physicsSystem](const std::function<Vec2()>& gen, Vec2 halfSize) -> std::pair<bool, Vec2>
	{
		Vec2 position;
		int safetyValve = 25;
		do
		{
			position = gen();
		} while (physicsSystem->MapSolid(Bounds2D::FromCenter(position, halfSize)) && --safetyValve > 0);
		if (safetyValve <= 0)
			debug::Log("Warning: unable to find safe spawn location within iteration limit.");
		return { safetyValve > 0, position };
	};

	auto randomMapPosition = [map, &rng]()
	{
		Bounds2D bounds = map::Get(map)->worldBounds;
		return Vec2{ rng.RangeF(bounds.Left(), bounds.Right()), rng.RangeF(bounds.Top(), bounds.Bottom()) };
	};

	//constexpr int SPAWNER_COUNT = 5; int ct = 0;
	//for (auto spawnerEntities = world.CreateEntities<SPAWNER_COUNT>(); Entity spawner : spawnerEntities)
	//{
	//	//if (auto [found, position] = findSafeSpot(randomMapPosition, vec2::Half); found)
	//	//{
	//	//	//world.AddComponents(spawner, Transform{position}, Spawner{ enemyPrefab, rng.RangeF(5.0f, 10.0f), rng.RangeF(1, 10), 6 });
	//	//	world.AddComponents(spawner,
	//	//		Transform{position},
	//	//		Spawner{ enemyPrefab, 0.5f, 0.5f, 4 });
	//	//}
	//	world.AddComponents(spawner,
	//		Transform{ {3 + 1.2f * ct, 2 + 1.6f * ct} },
	//		Spawner{ enemyPrefab, 3, 0, 2 });
	//	++ct;
	//}

	constexpr int ENEMY_COUNT = 0;
#if 1
	//for (int i = 0; i < ENEMY_COUNT; ++i)
	//{
	//	Entity enemy = world.CloneEntity(enemyPrefab);
	//	if (auto [found, position] = findSafeSpot(randomMapPosition, vec2::Half); found)
	//	{
	//		world.GetComponent<Transform>(enemy).position = position;
	//	}
	//}
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

	//cameraControlSystem->SnapFocusToFollow(cameraEntity);

	int targetFrames = 60;
	double targetFrameTime = 1.0 / targetFrames;
	debug::DevConsoleAddCommand("setfps", [&targetFrames, &targetFrameTime](int target)
		{
			target = std::max(target, 1);
			targetFrames = target;
			targetFrameTime = 1.0 / targetFrames;
			return targetFrames;
		});

	uint64_t startTime = stm_now();
	uint64_t clock = startTime;
	uint64_t lastFrameTime = clock;
	double secondTimer = 1.0;
	int frameCountThisSecond = 0;
	int fps = 0;
	uint64_t frameTicks = 0;
	ring_buf<uint64_t, 60> frameTickMeasures{};
	uint64_t averageFrameTick = 0;

	bool isRunning = true;
	bool showColliders = false;
#ifdef _DEBUG
	bool showDebugWatch = true;
#else
	bool showDebugWatch = false;
#endif
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

		*frameTickMeasures.next() = frameTicks;
		if (frameTickMeasures.index() == 0)
		{
			averageFrameTick = types::average(frameTickMeasures);
		}

		debug::Watch("FPS: {:d}, Frame: {:.3f}ms, Max: {:.3f}ms", fps, stm_ms(averageFrameTick), stm_ms(*std::ranges::max_element(frameTickMeasures)));
		debug::Watch("Entities: {:d}", world.GetEntityCount());

		GameTime gameTime(elapsedSec, deltaSec);

		expirationSystem->Update(gameTime);
		spawnerSystem->Update(gameTime);
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
		testSpawnSystem->Update(gameTime);
		testSystem->Update(gameTime);

		SpriteSheetViewControl(ssv);

		draw::Clear(drawContext);
		gameMapRenderSystem->RenderLayers(drawContext, std::array{ StrId("Background") });
		spriteRenderSystem->Render(drawContext);
		if (showColliders) debugMarkerSystem->DrawMarkers(drawContext);
		testSystem->Render(drawContext, viewSystem->ActiveCamera());

		SpriteSheetViewRender(drawContext, ssv);

		if (showDebugWatch)
		{
			debug::DrawWatch(drawContext, Color{ 0, 255, 0, 255 });
		}
		debug::DrawConsole(drawContext, gameTime.dt());

		SDL_RenderPresent(renderer);

		++frameCountThisSecond;

		frameTicks = stm_now() - clock;

		if (targetFrameTime > 0)
		{
			while (stm_sec(stm_diff(stm_now(), lastFrameTime)) < targetFrameTime) {}
			lastFrameTime = stm_now();
		}
	}

	PrintStringReport(StrId::QueryStringReport());
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
		debug::Log("ASSERT FAILED {:s} in {:s}:{:d}", exprStr, function, lineNum);
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