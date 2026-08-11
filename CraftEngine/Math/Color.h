#pragma once

#include <Core/Core.h>
#include <Windows.h>

namespace Craft
{
	// 색상을 열거형으로 정의.
	enum class CRAFT_API Color : WORD
	{
		Red = FOREGROUND_RED,
		Green = FOREGROUND_GREEN,
		Blue = FOREGROUND_BLUE,
		Yellow = Red | Green,
		Cyan = Green | Blue,
		Purple = Red | Blue,
		White = Red | Green | Blue,
		BrightWhite = White | FOREGROUND_INTENSITY
	};
}
