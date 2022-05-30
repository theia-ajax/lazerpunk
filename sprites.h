#pragma once

#include "types.h"

struct SDL_Surface;
struct SDL_Texture;
struct SDL_Renderer;

struct SpriteSheet
{
	SDL_Renderer* _renderer;
	SDL_Surface* _surface;
	SDL_Texture* _texture;
	int _width;
	int _height;
	int _pitch;
	int _padding;
	int _spriteWidth;
	int _spriteHeight;
	int _spriteRows;
	int _spriteCols;
};

struct SpriteRect
{
	int x, y, w, h;
};

enum class SpriteFlipFlags
{
	None = 0,
	FlipX = 1,
	FlipY = 2,
	FlipDiag = 4,
};

namespace sprite_sheet
{
	inline bool IsValid(const SpriteSheet& sheet) { return sheet._texture != nullptr; }
	inline int Rows(const SpriteSheet& sheet) { return sheet._spriteRows; }
	inline int Columns(const SpriteSheet& sheet) { return sheet._spriteCols; }
	inline int SpriteCount(const SpriteSheet& sheet) { return Rows(sheet) * Columns(sheet); }

	SpriteSheet Create(SDL_Renderer* renderer, const char* fileName, int spriteWidth, int spriteHeight, int padding);
	void Destroy(SpriteSheet* sheet);
	SpriteRect GetRect(const SpriteSheet& sheet, int spriteId, bool rotated = false);
	int GetSpriteId(const SpriteSheet& sheet, int spriteX, int spriteY);
}

namespace sprite
{
	void Draw(const SpriteSheet& sheet, int spriteId, float x, float y, float angle = 0.0f, SpriteFlipFlags flipFlags = SpriteFlipFlags::None, float originX = 0.0f, float originY = 0.0f);
}
