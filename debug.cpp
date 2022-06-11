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

	struct DevConsole
	{
		bool visible{};
		Vec2 position{};
		SDL_Surface* surface{};
		SDL_Surface* textSurface{};
		SDL_Texture* texture{};
	} console;
}

void debug::internal::WriteWatch(const std::string& message)
{
	s_watchQueue.emplace(message);
}

void debug::internal::WriteLog(const std::string& message)
{
	fprintf(stdout, "%s\n", message.c_str());
	s_log.emplace_back(message);
	if (console.textSurface)
	{
		SDL_FreeSurface(console.textSurface);
		console.textSurface = nullptr;
	}
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

	uint32_t wrapLen = static_cast<uint32_t>(ctx.canvas.x * 1.5f);
	SDL_Surface* textSurface = TTF_RenderText_Blended_Wrapped(ctx.font, buffer, SDL_Color{ 255, 255, 255, 255 }, wrapLen);

	if (!watchSurface)
	{
		watchSurface = SDL_CreateRGBSurfaceWithFormat(0, ctx.canvas.x * 2 + 4, ctx.canvas.y * 2 + 4, textSurface->format->BitsPerPixel, textSurface->format->format);
	}

	SDL_Rect bgRect{ 0, 0, textSurface->w + 4, textSurface->h + 4 };
	SDL_Rect textRect{ 0, 0, textSurface->w, textSurface->h };
	SDL_Rect textDstRect{ 2, 2, textSurface->w, textSurface->h };
	SDL_FillRect(watchSurface, nullptr, SDL_MapRGBA(watchSurface->format, 0, 0, 0, 0));
	SDL_FillRect(watchSurface, &bgRect, SDL_MapRGBA(watchSurface->format, 0, 255, 255, 64));
	SDL_BlitSurface(textSurface, &textRect, watchSurface, &textDstRect);
	SDL_FreeSurface(textSurface);

	if (!watchTexture)
	{
		watchTexture = SDL_CreateTextureFromSurface(ctx.renderer, watchSurface);
	}

	void* pixels; int pitch;
	SDL_LockTexture(watchTexture, nullptr, &pixels, &pitch);
	SDL_UpdateTexture(watchTexture, nullptr, watchSurface->pixels, watchSurface->pitch);
	SDL_UnlockTexture(watchTexture);

	SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
	SDL_Rect drawRect{ 0, 0, ctx.canvas.x / 3, ctx.canvas.y / 3 };
	SDL_RenderCopy(ctx.renderer, watchTexture, nullptr, &drawRect);
}

void debug::ToggleConsole()
{
	console.visible = !console.visible;
}

bool debug::ConsoleVisible()
{
	return console.visible;
}

void debug::DrawConsole(const DrawContext& ctx, float dt)
{
	SDL_Point canvas{ ctx.canvas.x * 4, ctx.canvas.y * 3 };

	float target = (!console.visible) ? 0.0f : static_cast<float>(canvas.y - 20);
	console.position.y = math::MoveTo(console.position.y, target, canvas.y * dt);

	if (!console.textSurface)
	{
		int32_t ssize = static_cast<int32_t>(s_log.size());
		int32_t start = std::max(ssize - 22, 0);
		std::vector<char> buffer(1024);
		char* head = buffer.data();
		size_t remaining = buffer.capacity();

		for (int32_t i = start; i < ssize && remaining > 0; ++i)
		{
			int32_t written = snprintf(head, remaining, "%s\n", s_log[i].c_str());

			if (written < 0)
				break;

			head += written;
			remaining -= written;
		}

		console.textSurface = TTF_RenderText_Blended_Wrapped(
			ctx.font, buffer.data(), { 255, 255, 255, 255 }, static_cast<uint32_t>(canvas.x * 3.0f / 4));
	}

	SDL_Rect canvasRect{ 0, 0, canvas.x, canvas.y };

	if (!console.surface)
	{
		console.surface = SDL_CreateRGBSurfaceWithFormat(0, canvas.x, canvas.y,
			console.textSurface->format->BitsPerPixel, console.textSurface->format->format);
	}

	SDL_Rect textRect{ 0, 0, console.textSurface->w, console.textSurface->h };
	SDL_Rect textDstRect{ 0, 4 * 16, textRect.w, textRect.h };
	SDL_FillRect(console.surface, &canvasRect, SDL_MapRGBA(console.surface->format, 0, 0, 0, 127));
	SDL_BlitSurface(console.textSurface, &textRect, console.surface, &textDstRect);

	if (!console.texture)
	{
		console.texture = SDL_CreateTextureFromSurface(ctx.renderer, console.surface);
	}

	void* pixels; int pitch;
	SDL_LockTexture(console.texture, nullptr, &pixels, &pitch);
	SDL_UpdateTexture(console.texture, nullptr, console.surface->pixels, console.surface->pitch);
	SDL_UnlockTexture(console.texture);

	SDL_Rect srcRect{ 0, 0, static_cast<int>(canvas.x * 3 / 4.0f), canvas.y};
	SDL_Rect drawRect{
		static_cast<int>(console.position.x), static_cast<int>(console.position.y - canvas.y),
		static_cast<int>(ctx.canvas.x * 3 / 4.0f), ctx.canvas.y};
	SDL_RenderCopy(ctx.renderer, console.texture, &srcRect, &drawRect);
}
