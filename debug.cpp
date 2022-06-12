#include "debug.h"

#include <format>
#include <fstream>
#include <queue>
#include <ranges>
#include <regex>
#include <sstream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "draw.h"
#include "string_util.h"
#include "types.h"

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
		SDL_Point canvas{};
		TTF_Font* font{};
		std::string input{};
		int32_t inputCursor{};
		int32_t inputSelectionLen{};
		SDL_Surface* inputSurface{};
		std::string inputCache{};
		float cursorTimer{};
		std::vector<SDL_Surface*> outputSurfaces{};
		std::vector<std::string> output{};
		SDL_Surface* surface{};
		SDL_Texture* texture{};
		debug::CommandProcessor commands;
	} sDevCon;

	void DevConsoleUpdateInputPromptSurface()
	{
		if (sDevCon.inputSurface)
			SDL_FreeSurface(sDevCon.inputSurface);

		std::string inputString = "> " + sDevCon.input;
		sDevCon.inputSurface = TTF_RenderText_Blended(sDevCon.font, inputString.c_str(), { 0, 255, 255, 255 });
	}

	void DevConsoleWrite(const std::string& message)
	{
		if (sDevCon.outputSurfaces.back() != nullptr)
		{
			SDL_FreeSurface(sDevCon.outputSurfaces.back());
			sDevCon.outputSurfaces.back() = nullptr;
		}

		for (size_t i = sDevCon.outputSurfaces.size() - 1; i > 0; --i)
		{
			sDevCon.outputSurfaces[i] = sDevCon.outputSurfaces[i - 1];
		}

		sDevCon.outputSurfaces[0] = TTF_RenderText_Blended_Wrapped(
			sDevCon.font, (!message.empty()) ? message.c_str() : "\n",
			{ 255, 255, 255, 255 },
			static_cast<uint32_t>(sDevCon.canvas.x));
	}

}

void debug::internal::WriteWatch(const std::string& message)
{
	s_watchQueue.emplace(message);
}

void debug::internal::WriteLog(const std::string& message)
{
	fprintf(stdout, "%s\n", message.c_str());
	s_log.emplace_back(message);
	DevConsoleWrite(message);
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
	std::stringstream watchBuffer;

	while (!s_watchQueue.empty())
	{
		std::string message = s_watchQueue.front();
		s_watchQueue.pop();
		watchBuffer << message << std::endl;
	}

	uint32_t wrapLen = static_cast<uint32_t>(ctx.canvas.x * 1.5f);
	SDL_Surface* textSurface = TTF_RenderText_Blended_Wrapped(
		ctx.font, watchBuffer.str().c_str(),
		SDL_Color{ 255, 255, 255, 255 }, wrapLen);

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

void debug::InitDevConsole(const DevConsoleConfig& config)
{
	sDevCon.canvas = { config.canvasX, config.canvasY };
	sDevCon.font = config.font;
	sDevCon.outputSurfaces.resize(config.historyLength);
	sDevCon.surface = SDL_CreateRGBSurfaceWithFormat(0, sDevCon.canvas.x, sDevCon.canvas.y, 32, SDL_PIXELFORMAT_ARGB8888);
	sDevCon.texture = SDL_CreateTextureFromSurface(config.renderer, sDevCon.surface);
	DevConsoleUpdateInputPromptSurface();
}


void debug::ToggleDevConsole()
{
	sDevCon.visible = !sDevCon.visible;

	if (sDevCon.visible)
	{
		SDL_StartTextInput();

	}
	else
	{
		SDL_StopTextInput();
	}
}

bool debug::IsConsoleVisible()
{
	return sDevCon.visible;
}

void Execute(const std::string& commandStr)
{
	std::vector<std::string> commandStrings;
	std::regex rx(R"(([^("|')]\S*|("|').+?("|'))\s*)");
	std::smatch match;
	std::string current = commandStr;
	while (std::regex_search(current, match, rx))
	{
		std::string argString = match[0];
		string_util::trim(argString, [](auto ch) { return std::isspace(ch) || ch == '\"' || ch == '\''; });

		commandStrings.emplace_back(argString);
		current = match.suffix().str();
	}

	std::string command = commandStrings[0];
	std::vector<std::string> params{};
	for (size_t i = 1; i < commandStrings.size(); ++i)
		params.emplace_back(commandStrings[i]);

	StrId cmdId(command);

	if (sDevCon.commands.callbacks.contains(cmdId))
	{
		try
		{
			sDevCon.commands.callbacks[cmdId].callParse(params);
		}
		catch (const std::out_of_range& e)
		{
			(void)e;
			debug::Log("Unable to match parameters to command '{}'.", command);
		}
	}
	else
	{
		debug::Log("Unrecognized command '{}'.", command);
	}
}

void debug::DevConsoleKeyInput(int32_t key, bool press, bool repeat)
{
	if (!sDevCon.visible)
		return;

	if (press)
	{
		switch (key)
		{
		case SDL_SCANCODE_BACKSPACE:
			if (sDevCon.inputCursor > 0)
			{
				sDevCon.input.erase(sDevCon.inputCursor - 1, 1);
				sDevCon.inputCursor--;
			}
			break;
		case SDL_SCANCODE_DELETE:
			if (!sDevCon.input.empty())
				sDevCon.input.erase(sDevCon.inputCursor, 1);
			break;
		case SDL_SCANCODE_RETURN:
			if (!sDevCon.input.empty())
				Execute(sDevCon.input);
			sDevCon.input.clear();
			break;
		case SDL_SCANCODE_LEFT:
			sDevCon.inputCursor--;
			break;
		case SDL_SCANCODE_RIGHT:
			sDevCon.inputCursor++;
			break;
		}

		sDevCon.cursorTimer = 0;
		sDevCon.inputCursor = std::clamp(sDevCon.inputCursor, 0, static_cast<int32_t>(sDevCon.input.size()));
	}
}

void debug::DevConsoleTextInput(char text[32])
{
	if (!sDevCon.visible)
		return;

	char* c = text;
	while (*c)
	{
		if (*c != '`')
		{
			sDevCon.input.insert(sDevCon.inputCursor, c);
			sDevCon.inputCursor++;
		}
		sDevCon.inputCursor = std::clamp(sDevCon.inputCursor, 0, static_cast<int32_t>(sDevCon.input.size()));
		c++;
	}
	sDevCon.cursorTimer = 0;
}

void debug::DevConsoleTextEdit(char text[32], int32_t start, int32_t length)
{
#if 0
	if (!console.visible)
		return;

	console.input.insert(start, text);
#endif
}

void debug::DevConsoleClear()
{
	for (auto& outputSurface : sDevCon.outputSurfaces)
	{
		if (outputSurface)
			SDL_FreeSurface(outputSurface);
		outputSurface = nullptr;
	}
}

void debug::DrawConsole(const DrawContext& ctx, float dt)
{
	if (!sDevCon.surface)
		return;

	float height = ctx.canvas.y * 3 / 4.0f;
	float target = (!sDevCon.visible) ? 0.0f : height;
	sDevCon.position.y = math::MoveTo(sDevCon.position.y, target, height * dt * 4);

	if (sDevCon.position.y < 0.1f && !sDevCon.visible)
		return;

	if (sDevCon.input != sDevCon.inputCache)
	{
		sDevCon.inputCache = sDevCon.input;
		DevConsoleUpdateInputPromptSurface();
	}

	SDL_Rect canvasRect{ 0, 0, sDevCon.canvas.x, sDevCon.canvas.y };

	SDL_FillRect(sDevCon.surface, &canvasRect, SDL_MapRGBA(sDevCon.surface->format, 0, 0, 0, 127));

	sDevCon.cursorTimer += dt;

	SDL_Rect promptRect{ 0, sDevCon.surface->h - 24, sDevCon.surface->w, sDevCon.surface->h };
	SDL_Rect promptTextRect = promptRect;
	promptTextRect.y += 4;
	SDL_FillRect(sDevCon.surface, &promptRect, SDL_MapRGBA(sDevCon.surface->format, 0, 0, 0, 255));
	SDL_Rect cursorRect{ sDevCon.inputCursor * 16 + 32, promptTextRect.y, 16, 16 };

	if (std::fmod(sDevCon.cursorTimer / 0.5f, 2.0f) < 1.0f)
		SDL_FillRect(sDevCon.surface, &cursorRect, SDL_MapRGBA(sDevCon.surface->format, 0, 0, 255, 255));
	SDL_BlitSurface(sDevCon.inputSurface, nullptr, sDevCon.surface, &promptTextRect);

	{
		int index = 0;
		int textY = promptRect.y;
		while (textY > 0 && sDevCon.outputSurfaces[index])
		{
			SDL_Surface* out = sDevCon.outputSurfaces[index++];
			textY -= out->h + 1;
			SDL_Rect dstRect{ 0, textY, out->w, out->h };
			SDL_BlitSurface(out, nullptr, sDevCon.surface, &dstRect);
		}
	}

	void* pixels; int pitch;
	SDL_LockTexture(sDevCon.texture, nullptr, &pixels, &pitch);
	SDL_UpdateTexture(sDevCon.texture, nullptr, sDevCon.surface->pixels, sDevCon.surface->pitch);
	SDL_UnlockTexture(sDevCon.texture);

	SDL_Rect srcRect{ 0, 0, sDevCon.canvas.x, sDevCon.canvas.y };
	SDL_Rect drawRect{
		static_cast<int>(sDevCon.position.x), static_cast<int>(sDevCon.position.y - ctx.canvas.y),
		ctx.canvas.x, ctx.canvas.y };
	SDL_RenderCopy(ctx.renderer, sDevCon.texture, nullptr, &drawRect);
}

debug::CommandProcessor& debug::DevConsoleGetCommandProcessor()
{
	return sDevCon.commands;
}
