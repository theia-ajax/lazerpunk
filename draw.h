#pragma once

#include "types.h"
#include "sprites.h"
#include <array>

struct SDL_Renderer;
struct SDL_FRect;
struct SDL_Rect;

struct DrawContext
{
	SDL_Renderer* renderer;
};

struct DrawRect { Vec2 pos, dim; };

namespace draw_rect
{
	DrawRect Create(const SDL_Rect& r);
	DrawRect Create(const SDL_FRect& r);
}

namespace draw
{
	void Sprite(const DrawContext& ctx,
		const SpriteSheet& sheet,
		int spriteId,
		Vec2 position,
		float angle = 0.0f,
		SpriteFlipFlags flipFlags = SpriteFlipFlags::None,
		Vec2 origin = Vec2{ 0, 0 },
		Vec2 scale = Vec2{ 1, 1 });

	void SetColor(const DrawContext& ctx, Color color);

	void Clear(const DrawContext& ctx, Color color = { 0, 0, 0, 255 });

	void Point(const DrawContext& ctx, Vec2 p);
	void Point(const DrawContext& ctx, Vec2 p, Color color);
	void Line(const DrawContext& ctx, Vec2 a, Vec2 b);
	void Line(const DrawContext& ctx, Vec2 a, Vec2 b, Color color);
	void Lines(const DrawContext& ctx, const Vec2* points, size_t count);
	void Lines(const DrawContext& ctx, const Vec2* points, size_t count, Color color);

	template <int N>
	void Lines(const DrawContext& ctx, const std::array<Vec2, N>& points)
	{
		Lines(ctx, points.data(), points.size());
	}

	template <int N>
	void Lines(const DrawContext& ctx, const std::array<Vec2, N>& points, Color color)
	{
		Lines(ctx, points.data(), points.size(), color);
	}

	void Rect(const DrawContext& ctx, Vec2 a, Vec2 b);
	void Rect(const DrawContext& ctx, const DrawRect& rect);
	void Rect(const DrawContext& ctx, Vec2 a, Vec2 b, Color color);
	void Rect(const DrawContext& ctx, const DrawRect& rect, Color color);
	void RectFill(const DrawContext& ctx, Vec2 a, Vec2 b);
	void RectFill(const DrawContext& ctx, const DrawRect& rect);
	void RectFill(const DrawContext& ctx, Vec2 a, Vec2 b, Color color);
	void RectFill(const DrawContext& ctx, const DrawRect& rect, Color color);
}