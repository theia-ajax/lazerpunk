#include "debug.h"

#include <format>
#include <fstream>
#include <queue>
#include "types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "draw.h"

namespace
{
	SDL_Surface* watchSurface{};
	SDL_Texture* watchTexture{};
	std::queue<std::string> s_watchQueue{};
	std::vector<std::string> s_log{};
}

void debug::Watch(const std::string& message)
{
	s_watchQueue.emplace(message);
}

void debug::Log(const char* context, const std::string& message)
{
	s_log.emplace_back(std::format("[{}] {}", context, message));
}

void debug::LogWriteToFile(const char* fileName)
{
	std::ofstream file(fileName);
	for (auto& log : s_log)
	{
		file << log << std::endl;
	}
}

void debug::DrawWatch(const DrawContext& ctx)
{
	constexpr size_t WATCH_BUFFER_SIZE = 1024;
	char buffer[WATCH_BUFFER_SIZE];
	size_t remaining = WATCH_BUFFER_SIZE;
	char* head = buffer;

	while (!s_watchQueue.empty())
	{
		std::string message = s_watchQueue.front();
		s_watchQueue.pop();

		int written = snprintf(head, remaining, "%s\n", message.c_str());

		if (written < 0)
			break;

		head += written;
		remaining -= written;
	}

	uint32_t wrapLen = static_cast<uint32_t>(ctx.canvas.x * 4);
	SDL_Surface* textSurface = TTF_RenderText_Blended_Wrapped(ctx.font, buffer, SDL_Color{ 255, 255, 255, 255 }, wrapLen);

	if (!watchSurface)
	{
		watchSurface = SDL_CreateRGBSurfaceWithFormat(0, ctx.canvas.x * 4, ctx.canvas.y * 4, textSurface->format->BitsPerPixel, textSurface->format->format);
	}

	SDL_Rect textRect{ 0, 0, textSurface->w, textSurface->h };
	SDL_FillRect(watchSurface, nullptr, SDL_MapRGBA(watchSurface->format, 0, 0, 0, 0));
	SDL_BlitSurface(textSurface, &textRect, watchSurface, &textRect);
	SDL_FreeSurface(textSurface);

	if (!watchTexture)
	{
		watchTexture = SDL_CreateTextureFromSurface(ctx.renderer, watchSurface);
	}

	void* pixels;
	int pitch;
	SDL_LockTexture(watchTexture, nullptr, &pixels, &pitch);

	SDL_Rect updateRect{ 0, 0, watchSurface->w, watchSurface->h };
	SDL_UpdateTexture(watchTexture, nullptr, watchSurface->pixels, watchSurface->pitch);

	SDL_UnlockTexture(watchTexture);

	SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
	SDL_Rect r{ 0, 0, ctx.canvas.x, ctx.canvas.y };
	SDL_RenderCopy(ctx.renderer, watchTexture, nullptr, nullptr);
}
