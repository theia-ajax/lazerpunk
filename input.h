#pragma once

#include <SDL2/SDL_scancode.h>
namespace input
{
	void BeginNewFrame();
	void KeyDownEvent(SDL_Scancode key, bool isPressed, bool isRepeat);
	bool GetKey(SDL_Scancode key);
	bool GetKeyDown(SDL_Scancode key);
	bool GetKeyUp(SDL_Scancode key);
	bool GetKeyRepeat(SDL_Scancode key);
}
