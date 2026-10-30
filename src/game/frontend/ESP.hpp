#pragma once
#include "game/gta/Player.hpp"

namespace YimMenu
{
	class ESP
	{
	public:
		static void Draw();      // render thread: only draws what RunScript worked out
		static void RunScript(); // game thread: works out what to draw (natives, entities)
	};
}