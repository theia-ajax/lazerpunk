#include "gamemap.h"
#include "draw.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <SDL2/SDL.h>
#include <unordered_map>
#include <queue>

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

	GameMapTileLayer ParseTileLayer(const rapidjson::Document& doc, const rapidjson::Document::ValueType& jsonLayer)
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

	GameMapObjectLayer ParseObjectLayer(const rapidjson::Document& doc, const rapidjson::Document::ValueType& jsonLayer)
	{
		GameMapLayerCommon common = ParseLayerCommonData(jsonLayer);

		ASSERT(jsonLayer.HasMember("objects"));
		ASSERT(jsonLayer["objects"].IsArray());
		ASSERT(jsonLayer.HasMember("visible"));
		ASSERT(jsonLayer.HasMember("draworder"));

		float tileWidth = doc["tilewidth"].GetFloat();
		float tileHeight = doc["tileheight"].GetFloat();
		Vec2 tileExtents{ tileWidth, tileHeight };

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

			result.color = Color{ red, blue, green, alpha };
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
			bool isPoint = obj.HasMember("point") && obj["point"].GetBool();
			bool isEllipse = obj.HasMember("ellipse") && obj["ellipse"].GetBool();
			bool isTile = obj.HasMember("gid");
			bool isPolyline = obj.HasMember("polyline") && obj["polyline"].IsArray();
			bool isPolygon = obj.HasMember("polygon") && obj["polygon"].IsArray();
			int64_t gid = (isTile) ? obj["gid"].GetInt64() : 0;

			Vec2 worldPos = Vec2{ posX, posY } / tileExtents;
			Vec2 worldDim = Vec2{ width, height } / tileExtents;

			GameMapObjectType objectType = GameMapObjectType::Quad;
			if (isPoint) objectType = GameMapObjectType::Point;
			else if (isEllipse) objectType = GameMapObjectType::Ellipse;
			else if (isTile) objectType = GameMapObjectType::Tile;
			else if (isPolygon) objectType = GameMapObjectType::Polygon;
			else if (isPolyline) objectType = GameMapObjectType::Polyline;

			int tileGlobalId;
			SpriteFlipFlags flipFlags;
			ParseIdAndFlip(gid, tileGlobalId, flipFlags);

			GameMapObject object{
				StrId(nameStr),
				StrId(typeStr),
				objectType,
				objectId,
				tileGlobalId,
				worldPos,
				worldDim,
				rotation,
				flipFlags,
				visible,
			};

			if (isPolygon || isPolyline)
			{
				const auto& pointsArray = isPolyline ? obj["polyline"] : obj["polygon"];

				for (rapidjson::SizeType j = 0; j < pointsArray.Capacity(); ++j)
				{
					const auto& pointObj = pointsArray[j];

					ASSERT(pointObj.IsObject());
					ASSERT(pointObj.HasMember("x"));
					ASSERT(pointObj.HasMember("y"));

					Vec2 point{
						pointObj["x"].GetFloat(),
						pointObj["y"].GetFloat(),
					};

					object.polyline.push_back(point / tileExtents);
				}

				if (isPolygon)
				{
					object.polyline.push_back(Vec2{});
				}
			}

			result.objects[i] = object;
		}

		return result;
	}

	GameMapLayer ParseLayer(const rapidjson::Document& doc, const rapidjson::Document::ValueType& jsonLayer);

	GameMapGroupLayer ParseGroupLayer(const rapidjson::Document& doc, const rapidjson::Document::ValueType& jsonLayer)
	{
		GameMapLayerCommon common = ParseLayerCommonData(jsonLayer);

		ASSERT(jsonLayer.HasMember("layers"));
		ASSERT(jsonLayer["layers"].IsArray());

		GameMapGroupLayer result{ common };

		auto layerArray = jsonLayer["layers"].GetArray();
		result.layers.resize(layerArray.Capacity());

		for (rapidjson::SizeType i = 0; i < layerArray.Capacity(); ++i)
		{
			result.layers[i] = ParseLayer(doc, layerArray[i]);
		}

		return result;
	}

	GameMapLayer ParseLayer(const rapidjson::Document& doc, const rapidjson::Document::ValueType& jsonLayer)
	{
		ASSERT(jsonLayer.HasMember("type"));
		ASSERT(jsonLayer["type"].IsString());

		StrId typeId(jsonLayer["type"].GetString());

		GameMapLayer result{};

		if (typeId == kLayerTypeTile)
		{
			result = ParseTileLayer(doc, jsonLayer);
		}
		else if (typeId == kLayerTypeObject)
		{
			result = ParseObjectLayer(doc, jsonLayer);
		}
		else if (typeId == kLayerTypeGroup)
		{
			result = ParseGroupLayer(doc, jsonLayer);
		}

		return result;
	}

	GameMap Load(const char* fileName)
	{
		std::ifstream inFileStream(fileName);
		rapidjson::IStreamWrapper streamWrapper(inFileStream);

		rapidjson::Document doc;
		doc.ParseStream(streamWrapper);

		ASSERT(doc.IsObject());
		ASSERT(doc.HasMember("layers"));
		ASSERT(doc["layers"].IsArray());
		ASSERT(doc["layers"].Capacity() > 0);

		GameMap result{ StrId(fileName) };

		for (rapidjson::SizeType i = 0; i < doc["layers"].Capacity(); ++i)
		{
			GameMapLayer layer = ParseLayer(doc, doc["layers"][i]);
			result.layers.push_back(layer);
		}

		return result;
	}



	class GameMapManager
	{
	public:
		static constexpr uint32_t kMaxLoadedMaps = 64;
		static constexpr GameMapHandle kInvalidHandle{ 0 };

	public:
		GameMapManager()
		{
			for (uint32_t i = 1; i <= kMaxLoadedMaps; ++i)
			{
				availableHandles.push(GameMapHandle{ i });
			}
		}

		GameMapHandle Create()
		{
			ASSERT(!availableHandles.empty() && "Ran out of available maps.");

			GameMapHandle handle = availableHandles.front();
			availableHandles.pop();
			return handle;
		}

		void Remove(GameMapHandle handle)
		{
			ASSERT(handle != kInvalidHandle && "Invalid handle");
			ASSERT(handle.handle <= kMaxLoadedMaps && "Invalid handle");
			GameMap& map = Get(handle);
			mapHandlesByName.erase(map.assetPathId);
			map = {};
			availableHandles.push(handle);
		}

		GameMap& Get(GameMapHandle handle)
		{
			ASSERT(handle != kInvalidHandle && "Invalid handle");
			ASSERT(handle.handle <= kMaxLoadedMaps && "Invalid handle");
			return mapsByHandle[handle.handle - 1];
		}

		GameMapHandle LoadOrGet(const char* fileName)
		{
			StrId assetPathId = StrId(fileName);

			if (mapHandlesByName.contains(assetPathId))
			{
				return mapHandlesByName[assetPathId];
			}

			GameMapHandle handle = Create();
			Get(handle) = Load(fileName);
			mapHandlesByName[assetPathId] = handle;
			return handle;
		}

	private:
		std::queue<GameMapHandle> availableHandles;
		std::array<GameMap, kMaxLoadedMaps + 1> mapsByHandle;
		std::unordered_map<StrId, GameMapHandle> mapHandlesByName;
	};

	GameMapManager mapManager;
}

GameMapHandle map::Load(const char* fileName)
{
	return mapManager.LoadOrGet(fileName);
}

void map::Reload(GameMapHandle handle)
{
	GameMap& gameMap = mapManager.Get(handle);
	gameMap = ::Load(gameMap.assetPathId.CStr());
}

GameMap& map::Get(GameMapHandle handle)
{
	return mapManager.Get(handle);
}

namespace
{
	struct BlendModeGuard  // NOLINT(cppcoreguidelines-special-member-functions)
	{
		explicit BlendModeGuard(SDL_Renderer* renderer)  // NOLINT(cppcoreguidelines-pro-type-member-init)
			: renderer(renderer)
		{
			SDL_GetRenderDrawBlendMode(renderer, &blendMode);
		}

		~BlendModeGuard()
		{
			SDL_SetRenderDrawBlendMode(renderer, blendMode);
		}

		SDL_Renderer* renderer;
		SDL_BlendMode blendMode;
	};

	void DrawObjectLayer(const DrawContext& ctx, const GameMapObjectLayer& layer, const Camera& camera, const SpriteSheet& sheet)
	{
		BlendModeGuard guard(ctx.renderer);

		SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);

		Vec2 cameraOffset = camera.position * camera.scale;

		draw::SetColor(ctx, layer.color);

		for (const auto& [nameId, typeId, objectType, objectId, tileGlobalId, position, dimensions, rotation, flipFlags, visible, polyline] : layer.objects)
		{
			if (!visible) continue;

			Vec2 screenPos = camera::WorldToScreen(camera, position);
			Vec2 screenDim = camera::WorldScaleToScreen(camera, dimensions);

			switch (objectType)
			{
			case GameMapObjectType::Quad:
				draw::RectFill(ctx, DrawRect{ screenPos, screenDim });
				break;
			case GameMapObjectType::Tile:
				draw::Sprite(ctx, sheet, tileGlobalId - 1, screenPos, rotation, flipFlags, Vec2{ 0.0f, 0.0f }, screenDim * sheet.spriteExtents);
				break;
			case GameMapObjectType::Ellipse:
				break;
			case GameMapObjectType::Point:
				draw::Point(ctx, screenPos);
				break;
			case GameMapObjectType::Polyline:
			case GameMapObjectType::Polygon:
			{
				std::vector<Vec2> points(polyline.size());
				size_t i = 0;
				for (auto p : polyline)
				{
					points[i++] = camera::WorldToScreen(camera, p + position);
				}
				draw::Lines(ctx, points.data(), points.size());
				break;
			}
			}
		}
	}

	void DrawTileLayer(const DrawContext& ctx, const GameMapTileLayer& layer, const Camera& camera, const SpriteSheet& sheet)
	{
		for (const auto& [tileGlobalId, position, flipFlags] : layer.tiles)
		{
			Vec2 screenPosition = camera::WorldToScreen(camera, position);
			draw::Sprite(ctx, sheet, tileGlobalId - 1, screenPosition, 0.0f, flipFlags);
		}
	}

	void DrawLayer(const DrawContext& ctx, const GameMapLayer& layer, const Camera& camera, const SpriteSheet& sheet);

	void DrawGroupLayer(const DrawContext& ctx, const GameMapGroupLayer& layer, const Camera& camera, const SpriteSheet& sheet)
	{
		for (const auto& subLayer : layer.layers)
		{
			DrawLayer(ctx, subLayer, camera, sheet);
		}
	}

	void DrawLayer(const DrawContext& ctx, const GameMapLayer& layer, const Camera& camera, const SpriteSheet& sheet)
	{
		std::visit([&ctx, &camera, &sheet](auto&& layer)
		{
			using T = std::decay_t<decltype(layer)>;
			if (layer.visible)
			{
				if constexpr (std::is_same_v<T, GameMapTileLayer>)
				{
					DrawTileLayer(ctx, layer, camera, sheet);
				}
				else if constexpr (std::is_same_v<T, GameMapObjectLayer>)
				{
					DrawObjectLayer(ctx, layer, camera, sheet);
				}
				else if constexpr (std::is_same_v<T, GameMapGroupLayer>)
				{
					DrawGroupLayer(ctx, layer, camera, sheet);
				}
				else
				{
					static_assert(false, "Non-exhaustive visitor.");
				}
			}
		}, layer);
	}

	constexpr StrId GetLayerNameId(GameMapLayer layer)
	{
		return std::visit([](auto&& arg) -> StrId
			{
				return arg.nameId;
			}, layer);
	}
}

void map::Draw(const DrawContext& ctx, const GameMap& map, const Camera& camera, const SpriteSheet& sheet)
{
	for (const auto& layer : map.layers)
	{
		DrawLayer(ctx, layer, camera, sheet);
	}
}

void map::DrawLayers(const DrawContext& ctx, const GameMap& map, const Camera& camera, const SpriteSheet& sheet, const StrId* layerNames, size_t count)
{
	for (const auto& layer : map.layers)
	{
		StrId nameId = GetLayerNameId(layer);
		const StrId* end = layerNames + count;
		if (auto find = std::find_if(layerNames, end, [nameId](auto&& id) {return id == nameId; }); find != end)
		{
			DrawLayer(ctx, layer, camera, sheet);
		}
	}
}

