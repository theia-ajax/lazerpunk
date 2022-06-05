#include "input.h"
#include <memory>

namespace input
{
	struct Input
	{
		struct KeyState
		{
			bool isDown;
			bool isRepeat;
		};

		struct Keys
		{
			KeyState currDown[SDL_NUM_SCANCODES];
			KeyState prevDown[SDL_NUM_SCANCODES];
		} keys;
	};

	namespace
	{
		Input input{};
	}


	void BeginNewFrame()
	{
		std::memcpy(input.keys.prevDown, input.keys.currDown, sizeof(input.keys.currDown));
	}

	void KeyDownEvent(SDL_Scancode key, bool isPressed, bool isRepeat)
	{
		input.keys.currDown[key] = { isPressed, isRepeat };
	}

	bool GetKey(SDL_Scancode key)
	{
		return input.keys.currDown[key].isDown;
	}

	bool GetKeyDown(SDL_Scancode key)
	{
		return input.keys.currDown[key].isDown && !input.keys.prevDown[key].isDown && !input.keys.currDown[key].isRepeat;
	}

	bool GetKeyUp(SDL_Scancode key)
	{
		return !input.keys.currDown[key].isDown && input.keys.prevDown[key].isDown;
	}

	bool GetKeyRepeat(SDL_Scancode key)
	{
		return input.keys.currDown[key].isDown && input.keys.currDown[key].isRepeat || GetKeyDown(key);
	}
}