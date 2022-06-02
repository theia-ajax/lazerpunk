#include "sprites.h"

#include <SDL2/SDL.h>
#include <stb_image.h>

uint32_t* GetSurfacePixel(const SDL_Surface* surface, int x, int y)
{
	return (uint32_t*)((uint8_t*)surface->pixels + (y * surface->pitch + x * surface->format->BytesPerPixel));
}

SpriteSheet sprite_sheet::Create(SDL_Renderer* renderer, const char* fileName, int spriteWidth, int spriteHeight, int padding)
{
	ASSERT(renderer);
	ASSERT(fileName);
	ASSERT(spriteWidth > 0);
	ASSERT(spriteHeight > 0);
	ASSERT(padding >= 0);

	SDL_Surface* imageSurface = nullptr;
	SDL_Texture* texture = nullptr;
	int sheetWidth, sheetHeight, _, sheetPitch = 0;
	SDL_Rect imageRect{};
	stbi_uc* data = nullptr;
	if ((data = stbi_load("assets/spritesheet.png", &sheetWidth, &sheetHeight, &_, 4)))
	{
		sheetPitch = sheetWidth * 4;
		imageSurface = SDL_CreateRGBSurfaceWithFormatFrom(data, sheetWidth, sheetHeight, 4, sheetPitch, SDL_PIXELFORMAT_ABGR8888);
		imageRect.w = sheetWidth;
		imageRect.h = sheetHeight;
	}
	else
	{
		return SpriteSheet{};
	}

	// The actual surface used to create the sprite sheet texture will be twice as tall as the image to accomodate a set of rotated sprites in the bottom half
	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, sheetWidth, sheetHeight * 2, sheetPitch, imageSurface->format->format);
	SDL_BlitSurface(imageSurface, nullptr, surface, &imageRect);

	auto pixelsNeeded = [](int count, int spriteSize, int spritePadding, int spriteMargin)
	{
		return count * spriteSize + ((count > 1) ? count : 0) * spritePadding + spriteMargin * 2;
	};

	auto findCount = [pixelsNeeded](int sheetSize, int spriteSize, int spritePadding, int spriteMargin)
	{
		int result = 0;
		while (pixelsNeeded(result, spriteSize, spritePadding, spriteMargin) < sheetSize) ++result;
		return result;
	};

	int spriteRows = findCount(sheetHeight, spriteHeight, padding, 0);
	int spriteCols = findCount(sheetWidth, spriteWidth, padding, 0);

	SpriteSheet result = SpriteSheet{
		._width = sheetWidth,
		._height = sheetHeight,
		._pitch = sheetPitch,
		._padding = padding,
		._spriteWidth = spriteWidth,
		._spriteHeight = spriteHeight,
		._spriteRows = spriteRows,
		._spriteCols = spriteCols,
	};

	// Copy each tile into the bottom half of the surface but rotated anti-clockwise 90 degrees.
	// This accomodates tiles which are "flipped diagnolly"
	for (int y = 0; y < spriteRows; ++y)
	{
		for (int x = 0; x < spriteCols; ++x)
		{
			int id = GetSpriteId(result, x, y);
			SpriteRect source = GetRect(result, id);
			SpriteRect dest = GetRect(result, id, true);

			for (int px = spriteWidth - 1; px >= 0; --px)
			{
				for (int py = 0; py < spriteHeight; ++py)
				{
					int sx = source.x + px;
					int sy = source.y + py;
					int dx = dest.x + py;
					int dy = dest.y + px;
					
					uint32_t* sourcePixel = GetSurfacePixel(surface, sx, sy);
					uint32_t* destPixel = GetSurfacePixel(surface, dx, dy);
					*destPixel = *sourcePixel;
				}
			}
		}
	}

	texture = SDL_CreateTextureFromSurface(renderer, surface);

	result._surface = surface;
	result._texture = texture;

	SDL_FreeSurface(imageSurface);
	stbi_image_free(data);

	return result;
}

void sprite_sheet::Destroy(SpriteSheet& sheet)
{
	if (sheet._texture)
	{
		SDL_DestroyTexture(sheet._texture);
		sheet._texture = nullptr;
	}
	if (sheet._surface)
	{
		SDL_FreeSurface(sheet._surface);
		sheet._surface = nullptr;
	}
}

SpriteRect sprite_sheet::GetRect(const SpriteSheet& sheet, int spriteId, bool diagnolFlip)
{
	if (spriteId < 0 || Rows(sheet) <= 0 || Columns(sheet) <= 0)
	{
		return SpriteRect{};
	}

	spriteId = spriteId % SpriteCount(sheet);

	int spriteRow = spriteId / sheet._spriteCols;
	int spriteCol = spriteId % sheet._spriteCols;

	int spriteX = spriteCol * sheet._spriteWidth + ((spriteCol > 0) ? sheet._padding * spriteCol : 0);
	int spriteY = spriteRow * sheet._spriteHeight + ((spriteRow > 0) ? sheet._padding * spriteRow : 0);

	if (diagnolFlip)
	{
		spriteY += sheet._height;
	}

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

void sprite::Draw(const DrawContext& ctx, const SpriteSheet& sheet, int spriteId, float x, float y, float angle, SpriteFlipFlags flipFlags, float originX, float originY, float scaleX, float scaleY)
{
	SpriteRect sourceRect = sprite_sheet::GetRect(sheet, spriteId, !!(flipFlags & SpriteFlipFlags::FlipDiag));
	SDL_FRect destRect{
		x - sheet._spriteWidth * (originX * scaleX),
		y - sheet._spriteHeight * (originY * scaleY),
		static_cast<float>(sheet._spriteWidth) * scaleX,
		static_cast<float>(sheet._spriteHeight) * scaleY
	};
	SDL_RendererFlip rendererFlip = ((SDL_RendererFlip)flipFlags) & (SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL);
	SDL_RenderCopyExF(ctx.renderer, sheet._texture, reinterpret_cast<SDL_Rect*>(&sourceRect), &destRect, angle, nullptr, rendererFlip);
}
