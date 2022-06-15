#pragma once

#include <memory>

#include "types.h"
#include "sprites.h"
#include <variant>
#include <vector>
#include <optional>

struct Camera;

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
	Polyline,
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
	std::vector<Vec2> polyline;
};

struct GameMapObjectLayer : GameMapLayerCommon
{
	GameMapObjectLayer() = default;
	explicit GameMapObjectLayer(const GameMapLayerCommon& common) : GameMapLayerCommon(common) {}

	Color color{};
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
	int tileWidth{};
	int tileHeight{};
	int tileCountX{};
	int tileCountY{};
	Bounds2D worldBounds;
	std::vector<GameMapLayer> layers;
};

struct GameMapHandle
{
	uint32_t handle;
	constexpr explicit operator bool() const { return handle != 0; }
};
constexpr bool operator==(const GameMapHandle& a, const GameMapHandle& b) { return a.handle == b.handle; }
constexpr bool operator!=(const GameMapHandle& a, const GameMapHandle& b) { return a.handle != b.handle; }

struct DrawContext;

namespace map
{
	GameMapHandle Load(const char* fileName);
	void Reload(GameMapHandle handle);
	GameMap* Get(GameMapHandle handle);
	GameMapLayer* GetLayer(GameMap* map, const char* layerName);

	template <typename, typename>
	constexpr bool is_one_of_variants_types = false;

	template <typename... Ts, typename T>
	constexpr bool is_one_of_variants_types<std::variant<Ts...>, T>
		= (std::is_same_v<T, Ts> || ...);

	template <typename T>
	auto GetLayer(GameMap* map, const char* layerName) -> std::enable_if_t<is_one_of_variants_types<GameMapLayer, T>, T*>
	{
		if (GameMapLayer* untypedLayer = GetLayer(map, layerName); untypedLayer)
		{
			return std::visit([](auto&& arg) -> T*
				{
					using LT = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, LT>)
					{
						return &arg;
					}
					return {};

				}, *untypedLayer);
		}
		return nullptr;
	}

	constexpr StrId GetLayerNameId(const GameMapLayer& layer)
	{
		return std::visit([](auto&& arg) -> StrId
			{
				return arg.nameId;
			}, layer);
	}

	void Draw(const DrawContext& ctx, const GameMap& map, const Camera& camera, const SpriteSheet& sheet);
	void DrawLayers(const DrawContext& ctx, const GameMap& map, const Camera& camera, const SpriteSheet& sheet, const StrId* layerNames, size_t count);

	template <int N>
	void DrawLayers(const DrawContext& ctx, const GameMap& map, const Camera& camera, const SpriteSheet& sheet, const std::array<StrId, N>& layers)
	{
		DrawLayers(ctx, map, camera, sheet, layers.data(), layers.size());
	}
}