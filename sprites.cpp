#include "sprites.h"

#include <filesystem>
#include <fstream>
#include <stb_image.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <SDL2/SDL.h>

constexpr SpriteRect kInvalidRect = SpriteRect{};
bool operator==(const SpriteRect& a, const SpriteRect& b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }
bool operator!=(const SpriteRect& a, const SpriteRect& b) { return !(a == b); }

uint32_t* GetSurfacePixel(const SDL_Surface* surface, int x, int y)
{
	return (uint32_t*)((uint8_t*)surface->pixels + (y * surface->pitch + x * surface->format->BytesPerPixel));
}

const SpriteRect& sprite_sheet::InvalidRect()
{
	return kInvalidRect;
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
	if ((data = stbi_load(fileName, &sheetWidth, &sheetHeight, &_, 4)))
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
		.width = sheetWidth,
		.height = sheetHeight,
		.pitch = sheetPitch,
		.padding = padding,
		.spriteWidth = spriteWidth,
		.spriteHeight = spriteHeight,
		.spriteRows = spriteRows,
		.spriteCols = spriteCols,
		.spriteExtents = Vec2{static_cast<float>(spriteWidth), static_cast<float>(spriteHeight)},
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

	result.surface = surface;
	result.texture = texture;

	SDL_FreeSurface(imageSurface);
	stbi_image_free(data);

	return result;
}

SpriteSheet sprite_sheet::Import(const char* sheetFileName, SDL_Renderer* renderer)
{
	std::ifstream inFileStream(sheetFileName);
	rapidjson::IStreamWrapper streamWrapper(inFileStream);

	rapidjson::Document doc;
	doc.ParseStream(streamWrapper);

	ASSERT(doc.HasMember("image"));
	const char* imageFilename = doc["image"].GetString();
	//int margin = doc["margin"].GetInt();
	int padding = doc["spacing"].GetInt();
	int tileWidth = doc["tilewidth"].GetInt();
	int tileHeight = doc["tileheight"].GetInt();

	auto path = std::filesystem::path(sheetFileName);
	auto imagePath = path.parent_path().append(std::string(imageFilename));
	auto imagePathStr = imagePath.string();

	return Create(renderer, imagePathStr.c_str(), tileWidth, tileHeight, padding);
}

void sprite_sheet::Destroy(SpriteSheet& sheet)
{
	if (sheet.texture)
	{
		SDL_DestroyTexture(sheet.texture);
		sheet.texture = nullptr;
	}
	if (sheet.surface)
	{
		SDL_FreeSurface(sheet.surface);
		sheet.surface = nullptr;
	}
}

SpriteRect sprite_sheet::GetRect(const SpriteSheet& sheet, int spriteId, bool diagnolFlip)
{
	if (spriteId < 0 || Rows(sheet) <= 0 || Columns(sheet) <= 0)
	{
		return SpriteRect{};
	}

	spriteId = spriteId % SpriteCount(sheet);

	int spriteRow = spriteId / sheet.spriteCols;
	int spriteCol = spriteId % sheet.spriteCols;

	int spriteX = spriteCol * sheet.spriteWidth + ((spriteCol > 0) ? sheet.padding * spriteCol : 0);
	int spriteY = spriteRow * sheet.spriteHeight + ((spriteRow > 0) ? sheet.padding * spriteRow : 0);

	if (diagnolFlip)
	{
		spriteY += sheet.height;
	}

	SpriteRect result = SpriteRect{
		.x = spriteX,
		.y = spriteY,
		.w = sheet.spriteWidth,
		.h = sheet.spriteHeight,
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
