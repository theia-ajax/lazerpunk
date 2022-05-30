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

	SDL_Surface* imageSurface = nullptr;
	SDL_Texture* texture = nullptr;
	int sheetWidth, sheetHeight, _, sheetPitch = 0;
	SDL_Rect imageRect{};
	stbi_uc* data = nullptr;
	if (data = stbi_load("assets/spritesheet.png", &sheetWidth, &sheetHeight, &_, 4))
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

	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, sheetWidth, sheetHeight * 2, sheetPitch, imageSurface->format->format);
	SDL_BlitSurface(imageSurface, nullptr, surface, &imageRect);

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
		._renderer = renderer,
		._width = sheetWidth,
		._height = sheetHeight,
		._pitch = sheetPitch,
		._padding = padding,
		._spriteWidth = spriteWidth,
		._spriteHeight = spriteHeight,
		._spriteRows = spriteRows,
		._spriteCols = spriteCols,
	};

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
					
					uint32_t* sourcePixel = (uint32_t*)((uint8_t*)surface->pixels + (sy * surface->pitch + sx * surface->format->BytesPerPixel));
					uint32_t* destPixel = (uint32_t*)((uint8_t*)surface->pixels + (dy * surface->pitch + dx * surface->format->BytesPerPixel));
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

SpriteRect sprite_sheet::GetRect(const SpriteSheet& sheet, int spriteId, bool rotated)
{
	if (spriteId < 0)
	{
		return SpriteRect{};
	}

	spriteId = spriteId % SpriteCount(sheet);

	int spriteRow = spriteId / sheet._spriteCols;
	int spriteCol = spriteId % sheet._spriteCols;

	int spriteX = spriteCol * sheet._spriteWidth + ((spriteCol > 0) ? sheet._padding * spriteCol : 0);
	int spriteY = spriteRow * sheet._spriteHeight + ((spriteRow > 0) ? sheet._padding * spriteRow : 0);

	if (rotated)
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

namespace internal
{
	void Draw(const SpriteSheet& sheet, int spriteId, float x, float y, float angle, SpriteFlipFlags flipFlags, float originX, float originY)
	{
		SpriteRect sourceRect = sprite_sheet::GetRect(sheet, spriteId, !!(flipFlags & SpriteFlipFlags::FlipDiag));

		SDL_FRect destRect{x - sheet._spriteWidth * originX, y - sheet._spriteHeight * originY, static_cast<float>(sheet._spriteWidth), static_cast<float>(sheet._spriteHeight)};

		SDL_RendererFlip rendererFlip = ((SDL_RendererFlip)flipFlags) & (SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL);

		SDL_RenderCopyExF(sheet._renderer, sheet._texture, reinterpret_cast<SDL_Rect*>(&sourceRect), &destRect, static_cast<double>(angle), nullptr, rendererFlip);
	}
}

void sprite::Draw(const SpriteSheet& sheet, int spriteId, float x, float y, float angle, SpriteFlipFlags flipFlags, float originX, float originY)
{
	internal::Draw(sheet, spriteId, x, y, angle, flipFlags, originX, originY);
}
