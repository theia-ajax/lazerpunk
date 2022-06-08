#pragma once

#include "types.h"

struct SDL_Surface;
struct SDL_Texture;
struct SDL_Renderer;
struct SDL_Rect;

struct SpriteSheet
{
	SDL_Surface* surface;
	SDL_Texture* texture;
	int width;
	int height;
	int pitch;
	int padding;
	int spriteWidth;
	int spriteHeight;
	int spriteRows;
	int spriteCols;
	Vec2 spriteExtents;
};

using SpriteRect = SDL_Rect;
bool operator==(const SpriteRect& a, const SpriteRect& b);
bool operator!=(const SpriteRect& a, const SpriteRect& b);

enum class SpriteFlipFlags
{
	None = 0,
	FlipX = 1,
	FlipY = 2,
	FlipDiag = 4,
};

namespace sprite_sheet
{
	inline bool IsValid(const SpriteSheet& sheet) { return sheet.texture != nullptr; }
	inline int Rows(const SpriteSheet& sheet) { return sheet.spriteRows; }
	inline int Columns(const SpriteSheet& sheet) { return sheet.spriteCols; }
	inline int SpriteCount(const SpriteSheet& sheet) { return Rows(sheet) * Columns(sheet); }

	const SpriteRect& InvalidRect();

	SpriteSheet Create(SDL_Renderer* renderer, const char* fileName, int spriteWidth, int spriteHeight, int padding);
	SpriteSheet Import(const char* sheetFileName, SDL_Renderer* renderer);
	void Destroy(SpriteSheet& sheet);
	SpriteRect GetRect(const SpriteSheet& sheet, int spriteId, bool diagnolFlip = false);
	int GetSpriteId(const SpriteSheet& sheet, int spriteX, int spriteY);
}

