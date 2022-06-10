#pragma once

#include <string>

struct DrawContext;

namespace debug
{
	void Watch(const std::string& message);
	void Log(const char* context, const std::string& message);
	void LogWriteToFile(const char* fileName);

	void DrawWatch(const DrawContext& ctx);
}
