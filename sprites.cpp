#include "sprites.h"

#include <SDL2/SDL.h>
#include <stb_image.h>

SpriteSheet sprite_sheet::Create(SDL_Renderer* renderer, const char* fileName, int spriteWidth, int spriteHeight, int padding)
{
	ASSERT(renderer);
	ASSERT(fileName);
	ASSERT(spriteWidth > 0);
	ASSERT(spriteHeight > 0);
	ASSERT(padding >= 0);

	SDL_Surface* surface = nullptr;
	SDL_Texture* texture = nullptr;
	int sheetWidth, sheetHeight, _, sheetPitch = 0;
	if (stbi_uc* data = stbi_load("assets/spritesheet.png", &sheetWidth, &sheetHeight, &_, 4))
	{
		sheetPitch = sheetWidth * 4;
		surface = SDL_CreateRGBSurfaceWithFormatFrom(data, sheetWidth, sheetHeight, 4, sheetPitch, SDL_PIXELFORMAT_ABGR8888);
		texture = SDL_CreateTextureFromSurface(renderer, surface);
		stbi_image_free(data);
	}
	else
	{
		return SpriteSheet{};
	}

	auto pixelsNeeded = [](int count, int spriteSize, int padding)
	{
		return count * spriteSize + ((count > 1) ? count : 0) * padding;
	};

	auto findCount = [pixelsNeeded](int sheetSize, int spriteSize, int padding)
	{
		int result = 0;
		while (pixelsNeeded(result, spriteSize, padding) < sheetSize) ++result;
		return result;
	};

	int spriteRows = findCount(sheetHeight, spriteHeight, padding);
	int spriteCols = findCount(sheetWidth, spriteWidth, padding);

	SpriteSheet result = SpriteSheet{
		._surface = surface,
		._texture = texture,
		._width = sheetWidth,
		._height = sheetHeight,
		._pitch = sheetPitch,
		._padding = padding,
		._spriteWidth = spriteWidth,
		._spriteHeight = spriteHeight,
		._spriteRows = spriteRows,
		._spriteCols = spriteCols,
	};
	return result;
}

void sprite_sheet::Destroy(SpriteSheet* sheet)
{
	if (sheet->_texture)
	{
		SDL_DestroyTexture(sheet->_texture);
		sheet->_texture = nullptr;
	}
	if (sheet->_surface)
	{
		SDL_FreeSurface(sheet->_surface);
		sheet->_surface = nullptr;
	}
}

SpriteRect sprite_sheet::GetRect(const SpriteSheet& sheet, int spriteId)
{
	if (!IsValid(sheet) || spriteId < 0)
	{
		return SpriteRect{};
	}

	spriteId = spriteId % SpriteCount(sheet);

	int spriteRow = spriteId / sheet._spriteCols;
	int spriteCol = spriteId % sheet._spriteCols;

	int spriteX = spriteCol * sheet._spriteWidth + ((spriteCol > 0) ? sheet._padding * spriteCol : 0);
	int spriteY = spriteRow * sheet._spriteHeight + ((spriteRow > 0) ? sheet._padding * spriteRow : 0);

	SpriteRect result = SpriteRect{
		.x = spriteX,
		.y = spriteY,
		.w = sheet._spriteWidth,
		.h = sheet._spriteHeight,
	};
	return result;
}

int sprite_sheet::GetSpriteId(const SpriteSheet& sheet, int spriteX, int spriteY)
{
	if (spriteX >= 0 && spriteX < Columns(sheet) && spriteY >= 0 && spriteY < Rows(sheet))
	{
		return spriteX + spriteY * Columns(sheet);
	}
	else
	{
		return -1;
	}
}

void sprite::Draw(SDL_Renderer* renderer, const SpriteSheet& sheet, int spriteId, float x, float y)
{
	constexpr float spriteScale = 1.0f;
	SpriteRect sourceRect = sprite_sheet::GetRect(sheet, spriteId);
	SDL_Rect destRect;
	destRect.x = static_cast<int>(x * spriteScale);
	destRect.y = static_cast<int>(y * spriteScale);
	destRect.w = static_cast<int>(sheet._spriteWidth * spriteScale);
	destRect.h = static_cast<int>(sheet._spriteHeight * spriteScale);
	SDL_RenderCopy(renderer, sheet._texture, reinterpret_cast<SDL_Rect*>(&sourceRect), &destRect);
}
