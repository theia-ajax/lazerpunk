
#include "types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "sokol_time.h"
#include "sprites.h"
#include <vector>
#include <fstream>
#include <variant>
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

inline Vec2 operator+(const Vec2& a, const Vec2& b) { return Vec2{ a.x + b.x, a.y + b.y }; }
inline Vec2 operator-(const Vec2& a, const Vec2& b) { return Vec2{ a.x - b.x, a.y - b.y }; }
inline Vec2 operator*(const Vec2& a, float b) { return Vec2{ a.x * b, a.y * b }; }
inline Vec2 operator/(const Vec2& a, float b) { return Vec2{ a.x / b, a.y / b }; }
inline bool operator==(const Vec2& a, const Vec2& b) { return a.x == b.x && a.y == b.y; }  // NOLINT(clang-diagnostic-float-equal)
inline bool operator!=(const Vec2& a, const Vec2& b) { return !(a == b); }  // NOLINT(clang-diagnostic-float-equal)

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

struct GameMapLayerCommon
{
	StrId nameId;
	int layerId{};
	bool visible{};
	Vec2 offset;
};

struct GameMapTile
{
	int tileGlobalId{};
	Vec2 position;
	SpriteFlipFlags flipFlags{};
};

struct GameMapTileLayer : public GameMapLayerCommon
{
	GameMapTileLayer() = default;
	explicit GameMapTileLayer(const GameMapLayerCommon& common) : GameMapLayerCommon(common) {}

	int tileCountX{};
	int tileCountY{};
	std::vector<GameMapTile> tiles;
};

enum class GameMapObjectType
{
	Quad,
	Ellipse,
	Point,
	Polygon,
	Tile,
};

enum class GameMapObjectDrawOrder
{
	TopDown,
	Index,
};

struct GameMapObject
{
	StrId nameId;
	StrId typeId;
	GameMapObjectType objectType{};
	int objectId{};
	int tileGlobalId{};
	Vec2 position;
	Vec2 dimensions;
	float rotation{};
	SpriteFlipFlags flipFlags{};
	bool visible{};
};

struct GameMapObjectLayer : GameMapLayerCommon
{
	GameMapObjectLayer() = default;
	explicit GameMapObjectLayer(const GameMapLayerCommon& common) : GameMapLayerCommon(common) {}

	SDL_Color color{};
	GameMapObjectDrawOrder drawOrder{};
	std::vector<GameMapObject> objects;
};

struct GameMapGroupLayer;
using GameMapLayer = std::variant<GameMapTileLayer, GameMapObjectLayer, GameMapGroupLayer>;

struct GameMapGroupLayer : GameMapLayerCommon
{
	std::vector<GameMapLayer> layers;
};


struct GameMap
{
	StrId assetPathId;
	std::vector<GameMapLayer> layers;
};

namespace map
{
	GameMap Load(const char* fileName);
	void Reload(GameMap& map);
	void Draw(const GameMap& map, const Camera& camera, const SpriteSheet& sheet);
}

struct Stopwatch
{
	const char* label;
	uint64_t start;
	explicit Stopwatch(const char* label) : label(label), start(stm_now()) {}
	~Stopwatch()
	{
		printf("STOPWATCH \'%s\' : %0.4fms\n", label, stm_ms(stm_diff(stm_now(), start)));
	}
};

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

	SpriteSheet sheet;
	{
		uint64_t s = stm_now();
		sheet = sprite_sheet::Create(renderer, "assets/spritesheet.png", 16, 16, 1);
		uint64_t diff = stm_diff(stm_now(), s);
		std::ofstream logFile("log.txt");
		logFile << stm_ms(diff);
	}

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
	return world * camera.pixelsToUnit - camera.position * camera.pixelsToUnit;
}

namespace
{
	const StrId kLayerTypeTile("tilelayer");
	const StrId kLayerTypeObject("objectgroup");
	const StrId kLayerTypeGroup("group");
	const StrId kDrawOrderTopDown("topdown");
	const StrId kDrawOrderIndex("index");

	GameMapLayerCommon ParseLayerCommonData(const rapidjson::Document::ValueType& jsonLayer)
	{
		ASSERT(jsonLayer.HasMember("name"));
		ASSERT(jsonLayer.HasMember("id"));
		ASSERT(jsonLayer.HasMember("visible"));
		ASSERT(jsonLayer.HasMember("x"));
		ASSERT(jsonLayer.HasMember("y"));

		StrId name = StrId(jsonLayer["name"].GetString());
		int id = jsonLayer["id"].GetInt();
		bool visible = jsonLayer["visible"].GetBool();
		float x = jsonLayer["x"].GetFloat();
		float y = jsonLayer["x"].GetFloat();
		GameMapLayerCommon common = { name, id, visible, Vec2{x, y} };
		return common;
	}

	inline void ParseIdAndFlip(int64_t data, int& id, SpriteFlipFlags& flipFlags)
	{
		id = data & 0x0FFFFFFF;
		flipFlags = ((data & 0x80000000) ? SpriteFlipFlags::FlipX : SpriteFlipFlags::None) |
			((data & 0x40000000) ? SpriteFlipFlags::FlipY : SpriteFlipFlags::None) |
			((data & 0x20000000) ? SpriteFlipFlags::FlipDiag : SpriteFlipFlags::None);
	}

	GameMapTileLayer ParseTileLayer(const rapidjson::Document::ValueType& jsonLayer)
	{
		GameMapLayerCommon common = ParseLayerCommonData(jsonLayer);

		ASSERT(jsonLayer.HasMember("width"));
		ASSERT(jsonLayer.HasMember("height"));
		ASSERT(jsonLayer.HasMember("data"));

		int width = jsonLayer["width"].GetInt();
		int height = jsonLayer["height"].GetInt();
		auto size = static_cast<rapidjson::SizeType>(width) * height;

		ASSERT(jsonLayer["data"].IsArray());
		ASSERT(jsonLayer["data"].Capacity() == size);

		GameMapTileLayer result{ common };

		result.tileCountX = width;
		result.tileCountY = height;
		result.tiles.resize(size);

		for (rapidjson::SizeType i = 0; i < size; ++i)
		{
			int64_t data = jsonLayer["data"][i].GetInt64();
			int id;
			SpriteFlipFlags flipFlags;
			ParseIdAndFlip(data, id, flipFlags);
			result.tiles[i] = GameMapTile{
				id,
				Vec2{static_cast<float>(i % result.tileCountX), static_cast<float>(i / result.tileCountX)},
				flipFlags,
			};
		}

		return result;
	}

	GameMapObjectLayer ParseObjectLayer(const rapidjson::Document::ValueType& jsonLayer)
	{
		GameMapLayerCommon common = ParseLayerCommonData(jsonLayer);

		ASSERT(jsonLayer.HasMember("objects"));
		ASSERT(jsonLayer["objects"].IsArray());
		ASSERT(jsonLayer.HasMember("visible"));
		ASSERT(jsonLayer.HasMember("draworder"));

		GameMapObjectLayer result{ common };

		GameMapObjectDrawOrder drawOrder{};
		StrId drawOrderStr = StrId(jsonLayer["draworder"].GetString());

		if (drawOrderStr == kDrawOrderIndex)
		{
			drawOrder = GameMapObjectDrawOrder::Index;
		}
		else
		{
			drawOrder = GameMapObjectDrawOrder::TopDown;
		}

		if (jsonLayer.HasMember("color"))
		{
			std::string colorString = jsonLayer["color"].GetString();

			ASSERT(colorString.length() == 7 || colorString.length() == 9);
			ASSERT(colorString[0] == '#');

			const bool hasAlpha = colorString.length() == 9;

			char redChars[3] = {}, blueChars[3] = {}, greenChars[3] = {}, alphaChars[3] = {};
			int csi = 1;
			if (hasAlpha)
			{
				alphaChars[0] = colorString[csi++];
				alphaChars[1] = colorString[csi++];
			}
			redChars[0] = colorString[csi++];
			redChars[1] = colorString[csi++];
			blueChars[0] = colorString[csi++];
			blueChars[1] = colorString[csi++];
			greenChars[0] = colorString[csi++];
			greenChars[1] = colorString[csi];

			uint8_t red = static_cast<uint8_t>(std::strtol(redChars, nullptr, 16));
			uint8_t blue = static_cast<uint8_t>(std::strtol(blueChars, nullptr, 16));
			uint8_t green = static_cast<uint8_t>(std::strtol(greenChars, nullptr, 16));
			uint8_t alpha = 255;

			if (hasAlpha)
			{
				alpha = static_cast<uint8_t>(std::strtol(alphaChars, nullptr, 16));
			}

			if (jsonLayer.HasMember("opacity"))
			{
				float opacity = jsonLayer["opacity"].GetFloat();
				alpha = static_cast<uint8_t>(static_cast<float>(alpha) * opacity);
			}

			result.color = SDL_Color{ red, blue, green, alpha };
		}


		auto objectArray = jsonLayer["objects"].GetArray();
		result.objects.resize(objectArray.Capacity());

		for (rapidjson::SizeType i = 0; i < objectArray.Capacity(); ++i)
		{
			const auto& obj = objectArray[i];
			bool visible = obj.HasMember("visible") && obj["visible"].GetBool();
			const char* nameStr = obj["name"].GetString();
			const char* typeStr = obj["type"].GetString();
			int objectId = obj["id"].GetInt();
			float posX = obj["x"].GetFloat();
			float posY = obj["y"].GetFloat();
			float width = obj["width"].GetFloat();
			float height = obj["height"].GetFloat();
			float rotation = obj.HasMember("rotation") ? obj["rotation"].GetFloat() : 0.0f;
			bool point = obj.HasMember("point") && obj["point"].GetBool();
			bool ellipse = obj.HasMember("ellipse") && obj["ellipse"].GetBool();
			bool tile = obj.HasMember("gid");
			bool polygon = obj.HasMember("polyline") && obj["polyline"].IsArray();
			int64_t gid = (tile) ? obj["gid"].GetInt64() : 0;

			GameMapObjectType objectType = GameMapObjectType::Quad;
			if (point) objectType = GameMapObjectType::Point;
			else if (ellipse) objectType = GameMapObjectType::Ellipse;
			else if (tile) objectType = GameMapObjectType::Tile;
			else if (polygon) objectType = GameMapObjectType::Polygon;

			int tileGlobalId;
			SpriteFlipFlags flipFlags;
			ParseIdAndFlip(gid, tileGlobalId, flipFlags);

			GameMapObject object{
				StrId(nameStr),
				StrId(typeStr),
				objectType,
				objectId,
				tileGlobalId,
				Vec2{posX, posY},
				Vec2{width, height},
				rotation,
				flipFlags,
				visible,
			};

			result.objects[i] = object;
		}

		return result;
	}

	GameMapLayer ParseLayer(const rapidjson::Document::ValueType& jsonLayer);

	GameMapGroupLayer ParseGroupLayer(const rapidjson::Document::ValueType& jsonLayer)
	{
		GameMapLayerCommon common = ParseLayerCommonData(jsonLayer);

		ASSERT(jsonLayer.HasMember("layers"));
		ASSERT(jsonLayer["layers"].IsArray());

		GameMapGroupLayer result{ common };

		auto layerArray = jsonLayer["layers"].GetArray();
		result.layers.resize(layerArray.Capacity());

		for (rapidjson::SizeType i = 0; i < layerArray.Capacity(); ++i)
		{
			result.layers[i] = ParseLayer(layerArray[i]);
		}

		return result;
	}

	GameMapLayer ParseLayer(const rapidjson::Document::ValueType& jsonLayer)
	{
		ASSERT(jsonLayer.HasMember("type"));
		ASSERT(jsonLayer["type"].IsString());

		StrId typeId(jsonLayer["type"].GetString());

		GameMapLayer result{};

		if (typeId == kLayerTypeTile)
		{
			result = ParseTileLayer(jsonLayer);
		}
		else if (typeId == kLayerTypeObject)
		{
			result = ParseObjectLayer(jsonLayer);
		}
		else if (typeId == kLayerTypeGroup)
		{
			result = ParseGroupLayer(jsonLayer);
		}

		return result;
	}
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

	GameMap result{StrId(fileName)};

	for (rapidjson::SizeType i = 0; i < doc["layers"].Capacity(); ++i)
	{
		GameMapLayer layer = ParseLayer(doc["layers"][i]);
		result.layers.push_back(layer);
	}

	return result;
}

void map::Reload(GameMap& map)
{
	map = map::Load(map.assetPathId.CStr());
}

namespace
{
	void DrawObjectLayer(const GameMapObjectLayer& layer, const Camera& camera, const SpriteSheet& sheet)
	{
		SDL_BlendMode blendMode;
		SDL_GetRenderDrawBlendMode(sheet._renderer, &blendMode);
		SDL_SetRenderDrawBlendMode(sheet._renderer, SDL_BLENDMODE_ADD);

		for (const auto& [nameId, typeId, objectType, objectId, tileGlobalId, position, dimensions, rotation, flipFlags, visible] : layer.objects)
		{
			if (!visible) continue;

			switch (objectType)
			{
			default:
			case GameMapObjectType::Quad:
				{
					auto [x, y] = position - camera.position * camera.pixelsToUnit;
					SDL_FRect objectRect{ x, y, dimensions.x, dimensions.y };

					SDL_SetRenderDrawColor(sheet._renderer, layer.color.r, layer.color.g, layer.color.b, layer.color.a);
					SDL_RenderFillRectF(sheet._renderer, &objectRect);
				}
				break;
			case GameMapObjectType::Tile:
				{
					auto [x, y] = position - camera.position * camera.pixelsToUnit;
					sprite::Draw(sheet, tileGlobalId - 1, x, y, rotation, flipFlags, 0.0f, 0.0f, dimensions.x / sheet._spriteWidth, dimensions.y / sheet._spriteHeight);
				}
				break;
			case GameMapObjectType::Ellipse: break;
			case GameMapObjectType::Point: break;
			case GameMapObjectType::Polygon: break;
			}
		}

		SDL_SetRenderDrawBlendMode(sheet._renderer, blendMode);
	}

	void DrawTileLayer(const GameMapTileLayer& layer, const Camera& camera, const SpriteSheet& sheet)
	{
		for (const auto& [tileGlobalId, position, flipFlags] : layer.tiles)
		{
			auto [x, y] = camera::WorldToScreen(camera, position);
			sprite::Draw(sheet, tileGlobalId - 1, x, y, 0.0f, flipFlags);
		}
	}

	void DrawLayer(const GameMapLayer& layer, const Camera& camera, const SpriteSheet& sheet);

	void DrawGroupLayer(const GameMapGroupLayer& layer, const Camera& camera, const SpriteSheet& sheet)
	{
		for (const auto& subLayer : layer.layers)
		{
			DrawLayer(subLayer, camera, sheet);
		}
	}

	void DrawLayer(const GameMapLayer& layer, const Camera& camera, const SpriteSheet& sheet)
	{
		std::visit([&camera, &sheet]<typename T0>(T0 && arg)
		{
			using T = std::decay_t<T0>;
			if (arg.visible)
			{
				if constexpr (std::is_same_v<T, GameMapTileLayer>)
				{
					DrawTileLayer(arg, camera, sheet);
				}
				else if constexpr (std::is_same_v<T, GameMapObjectLayer>)
				{
					DrawObjectLayer(arg, camera, sheet);
				}
				else if constexpr (std::is_same_v<T, GameMapGroupLayer>)
				{
					DrawGroupLayer(arg, camera, sheet);
				}
				else
				{
					static_assert(false, "Non-exhaustive visitor.");
				}
			}
		}, layer);
	}
}

void map::Draw(const GameMap& map, const Camera& camera, const SpriteSheet& sheet)
{
	for (const auto& layer : map.layers)
	{
		DrawLayer(layer, camera, sheet);
	}
}

namespace internal
{
	void PrintAssert(const char* function, int lineNum, const char* exprStr)
	{
		printf("ASSERT FAILED (%s) in %s:%d\n", exprStr, function, lineNum);
	}
}