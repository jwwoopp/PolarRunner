#include "TitleLevel.h"

#include <Engine/Engine.h>
#include <Input/Input.h>
#include <Level/PolarLevel.h>
#include <Math/Color.h>
#include <Render/Renderer.h>
#include <Windows.h>
#include <string>

void TitleLevel::Tick(float deltaTime)
{
	Level::Tick(deltaTime);

	const Craft::Input& input = Craft::Input::Get();
	if (input.GetKeyDown(VK_RETURN))
	{
		Craft::Engine::Get().AddNewLevel<PolarLevel>();
	}
	else if (input.GetKeyDown(VK_ESCAPE))
	{
		Craft::Engine::Get().Quit();
	}
}

void TitleLevel::Draw()
{
	const int screenWidth = Craft::Engine::Get().GetWidth();
	const int screenHeight = Craft::Engine::Get().GetHeight();
	const int centerX = screenWidth / 2;
	const int centerY = screenHeight / 2;

	// Centered title elements share the same positioning and sorting rule.
	const auto drawCenter = [&](const std::string& text, int yOffset,
		Craft::Color color)
	{
		const int posX = centerX - static_cast<int>(text.size()) / 2;
		const int posY = centerY + yOffset;
		Craft::Renderer::Get().Submit(text, Craft::Vector2(posX, posY),
			color, 3000);
	};

	drawCenter("POLAR RUNNER", -6, Craft::Color::BrightWhite);
	drawCenter(" _~_ ", -4, Craft::Color::Cyan);
	drawCenter("(o o)", -3, Craft::Color::BrightWhite);
	drawCenter("/ V \\", -2, Craft::Color::BrightWhite);
	drawCenter("SOUTH POLE  ->  NORTH POLE", 0, Craft::Color::Cyan);
	drawCenter("ENTER : START", 2, Craft::Color::Yellow);
	drawCenter("ESC : QUIT", 4, Craft::Color::BrightWhite);
}
