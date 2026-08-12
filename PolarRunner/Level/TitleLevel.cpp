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
	const int centerY = screenHeight / 2;
	const std::string title = "POLAR RUNNER";
	const std::string subtitle = "SOUTH POLE  ->  NORTH POLE";
	const std::string start = "ENTER : START";
	const std::string quit = "ESC : QUIT";

	Craft::Renderer::Get().Submit(title,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(title.size()) / 2,
			centerY - 6), Craft::Color::BrightWhite, 3000);
	Craft::Renderer::Get().Submit(" _~_ ",
		Craft::Vector2(screenWidth / 2 - 2, centerY - 4),
		Craft::Color::Cyan, 3000);
	Craft::Renderer::Get().Submit("(o o)",
		Craft::Vector2(screenWidth / 2 - 2, centerY - 3),
		Craft::Color::BrightWhite, 3000);
	Craft::Renderer::Get().Submit("/ V \\",
		Craft::Vector2(screenWidth / 2 - 2, centerY - 2),
		Craft::Color::BrightWhite, 3000);
	Craft::Renderer::Get().Submit(subtitle,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(subtitle.size()) / 2,
			centerY), Craft::Color::Cyan, 3000);
	Craft::Renderer::Get().Submit(start,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(start.size()) / 2,
			centerY + 2), Craft::Color::Yellow, 3000);
	Craft::Renderer::Get().Submit(quit,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(quit.size()) / 2,
			centerY + 4), Craft::Color::BrightWhite, 3000);
}
