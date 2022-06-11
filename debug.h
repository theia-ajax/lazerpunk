#pragma once

#include <string>
#include <format>

struct DrawContext;

namespace debug
{
	namespace internal
	{
		void WriteWatch(const std::string& message);
		void WriteLog(const std::string& message);
	}

	template <typename ...Args>
	void Watch(std::string_view fmt, Args&&... args)
	{
		internal::WriteWatch(std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...)));
	}

	template <typename ...Args>
	void Log(std::string_view fmt, Args&&... args)
	{
		internal::WriteLog(std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...)));
	}

	void LogWriteToFile(const char* fileName);

	void DrawWatch(const DrawContext& ctx);

	void ToggleConsole();
	bool ConsoleVisible();
	void DrawConsole(const DrawContext& ctx, float dt);
}
