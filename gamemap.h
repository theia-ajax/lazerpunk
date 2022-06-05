#pragma once

#include "types.h"
#include "sprites.h"
#include <variant>
#include <vector>

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
	std::vector<GameMapLayer> layers;
};

struct GameMapHandle { uint32_t handle; };
constexpr bool operator==(const GameMapHandle& a, const GameMapHandle& b) { return a.handle == b.handle; }
constexpr bool operator!=(const GameMapHandle& a, const GameMapHandle& b) { return a.handle != b.handle; }

struct DrawContext;

namespace map
{
	GameMapHandle Load(const char* fileName);
	void Reload(GameMapHandle handle);
	GameMap& Get(GameMapHandle handle);

	void Draw(const DrawContext& ctx, const GameMap& map, const Camera& camera, const SpriteSheet& sheet);
	void DrawLayers(const DrawContext& ctx, const GameMap& map, const Camera& camera, const SpriteSheet& sheet, const StrId* layerNames, size_t count);

	template <int N>
	void DrawLayers(const DrawContext& ctx, const GameMap& map, const Camera& camera, const SpriteSheet& sheet, const std::array<StrId, N>& layers)
	{
		DrawLayers(ctx, map, camera, sheet, layers.data(), layers.size());
	}

}