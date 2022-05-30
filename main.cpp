
#include "types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "sokol_time.h"
#include "sprites.h"
#include <vector>
#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

namespace input
{
	void BeginNewFrame();
	void KeyDownEvent(SDL_Scancode key, bool isPressed, bool isRepeat);
	bool GetKey(SDL_Scancode key);
	bool GetKeyDown(SDL_Scancode key);
	bool GetKeyUp(SDL_Scancode key);
	bool GetKeyRepeat(SDL_Scancode key);
}

struct Vec2
{
	float x = 0.0f, y = 0.0f;
};

struct Point2I
{
	int x = 0, y = 0;
};

inline int clamp(int v, int min, int max) { return (v < min) ? min : ((v > max) ? max : v); }

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

void DrawBox(SDL_Renderer* renderer, const SDL_Rect* rect);

constexpr uint64_t TicksInSecond(double sec) {
	return (uint64_t)(sec * 1000000000.0);
}

struct GameMapTile
{
	int id;
	bool flipX;
	bool flipY;
	bool flipDiag;
};

struct GameMapTileLayer
{
	int width;
	int height;
	std::vector<GameMapTile> tiles;
};

struct GameMap
{
	GameMapTileLayer layer;
};

namespace map
{
	GameMap Load(const char* fileName);
	void Draw(const GameMap& map, const Camera& camera, const SpriteSheet& sheet);
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

	SpriteSheet sheet = sprite_sheet::Create(renderer, "assets/spritesheet.png", 16, 16, 1);

	stm_setup();

	uint64_t startTime = stm_now();
	uint64_t clock = startTime;
	constexpr double kFixedTimeStepSec = 1.0 / 60.0;
	constexpr uint64_t kFixedTimeStepTicks = TicksInSecond(kFixedTimeStepSec);
	uint64_t lastFixedUpdate = startTime;

	GameMap map = map::Load("assets/testmap.tmj");

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

		uint64_t deltaTime = stm_laptime(&clock);
		uint64_t elapsed = stm_diff(stm_now(), startTime);
		double elapsedSec = stm_sec(elapsed);
		double deltaSec = stm_sec(deltaTime);

		GameTime gameTime(elapsedSec, deltaSec);
		game::Update(state, gameTime);

		if (elapsed >= lastFixedUpdate + kFixedTimeStepTicks)
		{
			lastFixedUpdate += kFixedTimeStepTicks;
			// fixed update here... 
		}

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

		game::Render(state, gameTime);

		if (showSpriteSheet)
		{
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);

			for (int y = 0; y < sprite_sheet::Rows(sheet); ++y)
			{
				for (int x = 0; x < sprite_sheet::Columns(sheet); ++x)
				{
					int spriteId = sprite_sheet::GetSpriteId(sheet, x, y);
					sprite::Draw(sheet, spriteId, (float)x * sheet._spriteWidth + ssvOffset.x, (float)y * sheet._spriteHeight + ssvOffset.y, 0.0, SpriteFlipFlags::None);
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

void DrawBox(SDL_Renderer* renderer, const SDL_Rect* rect)
{
	SDL_Point points[5] = {
		{ rect->x, rect->y },
		{ rect->x + rect->w, rect->y },
		{ rect->x + rect->w, rect->y + rect->h },
		{ rect->x, rect->y + rect->h },
		{ rect->x, rect->y },
	};
	SDL_RenderDrawLines(renderer, points, 5);
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

Vec2 camera::WorldToScreen(const Camera& camera, Vec2 world)
{
	Vec2 result;
	result.x = world.x * camera.pixelsToUnit - camera.position.x * camera.pixelsToUnit;
	result.y = world.y * camera.pixelsToUnit - camera.position.y * camera.pixelsToUnit;
	return result;
}

GameMap map::Load(const char* fileName)
{
	std::ifstream inFileStream(fileName);
	rapidjson::IStreamWrapper streamWrapper(inFileStream);

	rapidjson::Document doc;
	doc.ParseStream(streamWrapper);

	ASSERT(doc.IsObject());
	ASSERT(doc.HasMember("layers"));
	ASSERT(doc["layers"].IsArray());
	ASSERT(doc["layers"].Capacity() > 0);

	ASSERT(doc["layers"][0].HasMember("width"));
	ASSERT(doc["layers"][0].HasMember("height"));
	ASSERT(doc["layers"][0].HasMember("data"));

	int width = doc["layers"][0]["width"].GetInt();
	int height = doc["layers"][0]["height"].GetInt();

	size_t size = static_cast<size_t>(width * height);

	ASSERT(doc["layers"][0]["data"].IsArray());
	ASSERT(doc["layers"][0]["data"].Capacity() == size);

	GameMap result{};

	result.layer.width = width;
	result.layer.height = height;
	result.layer.tiles.resize(size);

	for (rapidjson::SizeType i = 0; i < size; ++i)
	{
		//auto element = doc["layers"][0]["data"][i];
		int64_t data = doc["layers"][0]["data"][i].GetInt64();
		int id = data & 0x0FFFFFFF;
		int flipFlags = static_cast<int>(data >> 28);
		bool flipX = (data & 0x80000000);
		bool flipY = (data & 0x40000000);
		bool flipDiag = (data & 0x20000000);
		result.layer.tiles[i] = GameMapTile{ id, flipX, flipY, flipDiag };
	}

	return result;
}

void map::Draw(const GameMap& map, const Camera& camera, const SpriteSheet& sheet)
{
	static float aa = 0.0f;
	aa += 0.1f;

	for (rapidjson::SizeType i = 0; i < map.layer.tiles.size(); ++i)
	{
		GameMapTile tile = map.layer.tiles[i];

		if (tile.id > 0)
		{
			int tx = i % map.layer.width;
			int ty = i / map.layer.width;
			Vec2 worldPos{ static_cast<float>(tx) , static_cast<float>(ty) };
			Vec2 screenPos = camera::WorldToScreen(camera, worldPos);

			SpriteFlipFlags flipFlags = SpriteFlipFlags::None;
			if (tile.flipX) flipFlags |= SpriteFlipFlags::FlipX;
			if (tile.flipY) flipFlags |= SpriteFlipFlags::FlipY;
			if (tile.flipDiag) flipFlags |= SpriteFlipFlags::FlipDiag;
			sprite::Draw(sheet, tile.id - 1, screenPos.x, screenPos.y, 0.0f, flipFlags);
		}
	}
}

namespace internal
{
	void PrintAssert(const char* function, int lineNum, const char* exprStr)
	{
		printf("ASSERT FAILED (%s) in %s:%d\n", exprStr, function, lineNum);
	}
}