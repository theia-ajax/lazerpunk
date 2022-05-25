#pragma once

#include "types.h"

struct SDL_Surface;
struct SDL_Texture;
struct SDL_Renderer;

struct SpriteSheet
{
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

namespace sprite_sheet
{
	SpriteSheet Create(SDL_Renderer* renderer, const char* fileName, int spriteWidth, int spriteHeight, int padding);
	void Destroy(SpriteSheet* sheet);
	SpriteRect GetRect(const SpriteSheet& sheet, int spriteId);
	int GetSpriteId(const SpriteSheet& sheet, int spriteX, int spriteY);

	inline bool IsValid(const SpriteSheet& sheet) { return sheet._texture != nullptr; }
	inline int Rows(const SpriteSheet& sheet) { return sheet._spriteRows; }
	inline int Columns(const SpriteSheet& sheet) { return sheet._spriteCols; }
	inline int SpriteCount(const SpriteSheet& sheet) { return Rows(sheet) * Columns(sheet); }
}

namespace sprite
{
	void Draw(SDL_Renderer* renderer, const SpriteSheet& sheet, int spriteId, float x, float y);
}
