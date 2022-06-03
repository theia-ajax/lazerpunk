#include "draw.h"

#include <SDL2/SDL.h>

namespace 
{
	constexpr SDL_FRect FRectFromDrawRect(const DrawRect& r)
	{
		return SDL_FRect{
			r.pos.x, r.pos.y, r.dim.x, r.dim.y
		};
	}
}

void draw::Sprite(const DrawContext& ctx, const SpriteSheet& sheet, int spriteId, Vec2 position, float angle, SpriteFlipFlags flipFlags, Vec2 origin, Vec2 scale)
{
	SpriteRect sourceRect = sprite_sheet::GetRect(sheet, spriteId, flags::Test(flipFlags, SpriteFlipFlags::FlipDiag));

	if (sourceRect == sprite_sheet::InvalidRect())
		return;

	auto [x, y] = position - sheet.spriteExtents * origin * scale;
	auto [w, h] = sheet.spriteExtents * scale;
	SDL_FRect destRect{ x, y, w, h };
	SDL_RendererFlip rendererFlip = (static_cast<SDL_RendererFlip>(flipFlags)) & (SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL);
	SDL_RenderCopyExF(ctx.renderer, sheet.texture, &sourceRect, &destRect, angle, nullptr, rendererFlip);
}

void draw::SetColor(const DrawContext& ctx, Color color)
{
	SDL_SetRenderDrawColor(ctx.renderer, color.r, color.g, color.b, color.a);
}

void draw::Clear(const DrawContext& ctx, Color color)
{
	SetColor(ctx, color);
	SDL_RenderClear(ctx.renderer);
}

void draw::Point(const DrawContext& ctx, Vec2 p)
{
	SDL_RenderDrawPointF(ctx.renderer, p.x, p.y);
}

void draw::Point(const DrawContext& ctx, Vec2 p, Color color)
{
	SetColor(ctx, color);
	Point(ctx, p);
}

void draw::Line(const DrawContext& ctx, Vec2 a, Vec2 b)
{
	SDL_RenderDrawLineF(ctx.renderer, a.x, a.y, b.x, b.y);
}

void draw::Line(const DrawContext& ctx, Vec2 a, Vec2 b, Color color)
{
	SetColor(ctx, color);
	Line(ctx, a, b);
}

void draw::Lines(const DrawContext& ctx, const Vec2* points, size_t count)
{
	SDL_RenderDrawLinesF(ctx.renderer, reinterpret_cast<const SDL_FPoint*>(points), static_cast<int>(count));
}

void draw::Lines(const DrawContext& ctx, const Vec2* points, size_t count, Color color)
{
	SetColor(ctx, color);
	Lines(ctx, points, count);
}

void draw::Rect(const DrawContext& ctx, Vec2 a, Vec2 b)
{
	Rect(ctx, DrawRect{ a, b - a });
}

void draw::Rect(const DrawContext& ctx, const DrawRect& rect)
{
	SDL_FRect r = FRectFromDrawRect(rect);
	SDL_RenderDrawRectF(ctx.renderer, &r);
}

void draw::Rect(const DrawContext& ctx, Vec2 a, Vec2 b, Color color)
{
	SetColor(ctx, color);
	Rect(ctx, a, b);
}

void draw::Rect(const DrawContext& ctx, const DrawRect& rect, Color color)
{
	SetColor(ctx, color);
	Rect(ctx, rect);
}

void draw::RectFill(const DrawContext& ctx, Vec2 a, Vec2 b)
{
	RectFill(ctx, DrawRect{ a, b - a });
}

void draw::RectFill(const DrawContext& ctx, const DrawRect& rect)
{
	SDL_FRect r = FRectFromDrawRect(rect);
	SDL_RenderFillRectF(ctx.renderer, &r);
}

void draw::RectFill(const DrawContext& ctx, Vec2 a, Vec2 b, Color color)
{
	SetColor(ctx, color);
	RectFill(ctx, a, b);
}

void draw::RectFill(const DrawContext& ctx, const DrawRect& rect, Color color)
{
	SetColor(ctx, color);
	RectFill(ctx, rect);
}

DrawRect draw_rect::Create(const SDL_Rect& r)
{
	return DrawRect{
		vec2::Create(r.x, r.y),
		vec2::Create(r.w, r.h),
	};
}

DrawRect draw_rect::Create(const SDL_FRect& r)
{
	return DrawRect{
		Vec2{r.x, r.y},
		Vec2{r.w, r.h},
	};
}
