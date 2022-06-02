#include "gamemap.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <SDL2/SDL.h>

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
			bool isPolygon = obj.HasMember("polyline") && obj["polyline"].IsArray();
			int64_t gid = (isTile) ? obj["gid"].GetInt64() : 0;

			GameMapObjectType objectType = GameMapObjectType::Quad;
			if (isPoint) objectType = GameMapObjectType::Point;
			else if (isEllipse) objectType = GameMapObjectType::Ellipse;
			else if (isTile) objectType = GameMapObjectType::Tile;
			else if (isPolygon) objectType = GameMapObjectType::Polygon;

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

			if (isPolygon)
			{
				for (rapidjson::SizeType j = 0; j < obj["polyline"].Capacity(); ++j)
				{
					const auto& pointObj = obj["polyline"][j];

					ASSERT(pointObj.IsObject());
					ASSERT(pointObj.HasMember("x"));
					ASSERT(pointObj.HasMember("y"));

					Vec2 point{
						pointObj["x"].GetFloat(),
						pointObj["y"].GetFloat(),
					};

					object.polyline.push_back(point);
				}
			}

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

	GameMap result{ StrId(fileName) };

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
		//SDL_SetRenderDrawBlendMode(sheet._renderer, SDL_BLENDMODE_ADD);

		Vec2 cameraOffset = camera.position * camera.pixelsToUnit;

		SDL_SetRenderDrawColor(sheet._renderer, layer.color.r, layer.color.g, layer.color.b, layer.color.a);

		for (const auto& [nameId, typeId, objectType, objectId, tileGlobalId, position, dimensions, rotation, flipFlags, visible, polyline] : layer.objects)
		{
			if (!visible) continue;

			switch (objectType)
			{
			case GameMapObjectType::Quad:
			{
				auto [x, y] = position - cameraOffset;
				SDL_FRect objectRect{ x, y, dimensions.x, dimensions.y };
				SDL_RenderFillRectF(sheet._renderer, &objectRect);
				break;
			}
			case GameMapObjectType::Tile:
			{
				auto [x, y] = position - cameraOffset;
				sprite::Draw(sheet, tileGlobalId - 1, x, y, rotation, flipFlags, 0.0f, 0.0f, dimensions.x / sheet._spriteWidth, dimensions.y / sheet._spriteHeight);
				break;
			}
			case GameMapObjectType::Ellipse:
				break;
			case GameMapObjectType::Point:
			{
				auto [x, y] = position - cameraOffset;
				SDL_RenderDrawPointF(sheet._renderer, x, y);
				break;
			}
			case GameMapObjectType::Polygon:
			{
				std::vector<SDL_FPoint> points(polyline.size());
				size_t i = 0;
				for (auto p : polyline)
				{
					auto [x, y] = p + position - cameraOffset;
					points[i++] = SDL_FPoint{ x, y };
				}
				SDL_RenderDrawLinesF(sheet._renderer, points.data(), static_cast<int>(points.size()));
				break;
			}
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

	constexpr StrId GetLayerNameId(GameMapLayer layer)
	{
		return std::visit([](auto&& arg) -> StrId
			{
				return arg.nameId;
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

void map::DrawLayers(const GameMap& map, const Camera& camera, const SpriteSheet& sheet, StrId* layerNames, size_t count)
{
	for (const auto& layer : map.layers)
	{
		StrId nameId = GetLayerNameId(layer);
		StrId* end = layerNames + count;
		if (auto find = std::find_if(layerNames, end, [nameId](auto&& id) {return id == nameId; }); find != end)
		{
			DrawLayer(layer, camera, sheet);
		}
	}
}

