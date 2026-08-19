#include "MenuLevel.h"

#include <Level/PolarLevel.h>
#include <Engine/Engine.h>
#include <Input/Input.h>
#include <Math/Color.h>
#include <Render/Renderer.h>
#include <Windows.h>

MenuLevel::MenuLevel(bool canResume, Item defaultItem)
{
	items.push_back({ std::string(),
		[]() { Craft::Engine::Get().SetSecondaryLevelActive(false); } });
	items.push_back({ "RETRY",
		[]()
		{
			Craft::Engine::Get().SetSecondaryLevelActive(false);
			Craft::Engine::Get().AddNewLevel<PolarLevel>();
		} });
	items.push_back({ "QUIT",
		[]() { Craft::Engine::Get().Quit(); } });

	SetContext(canResume, defaultItem);
}

void MenuLevel::SetContext(bool canResumeIn, Item defaultItem)
{
	canResume = canResumeIn;
	items[static_cast<int>(Item::Resume)].text = canResume ? "RESUME" : "BACK";
	currentIndex = static_cast<int>(defaultItem);
}

void MenuLevel::Tick(float deltaTime)
{
	Level::Tick(deltaTime);

	const Craft::Input& input = Craft::Input::Get();
	if (input.GetKeyDown(VK_ESCAPE))
	{
		Craft::Engine::Get().SetSecondaryLevelActive(false);
		return;
	}

	const int length = static_cast<int>(items.size());
	if (input.GetKeyDown(VK_UP))
	{
		currentIndex = (currentIndex - 1 + length) % length;
	}
	else if (input.GetKeyDown(VK_DOWN))
	{
		currentIndex = (currentIndex + 1) % length;
	}

	if (input.GetKeyDown(VK_RETURN))
	{
		items[currentIndex].onSelected();
	}
}

void MenuLevel::Draw()
{
	const int screenWidth = Craft::Engine::Get().GetWidth();
	const int screenHeight = Craft::Engine::Get().GetHeight();
	const int centerY = screenHeight / 2;

	const std::string title = canResume ? "PAUSED" : "MENU";
	Craft::Renderer::Get().Submit(title,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(title.size()) / 2,
			centerY - 3), Craft::Color::Yellow, 3000);

	const int count = static_cast<int>(items.size());
	for (int index = 0; index < count; ++index)
	{
		const bool selected = index == currentIndex;
		const std::string line = std::string(selected ? "> " : "  ")
			+ items[index].text + (selected ? " <" : "");
		Craft::Renderer::Get().Submit(line,
			Craft::Vector2(screenWidth / 2 - static_cast<int>(line.size()) / 2,
				centerY - 1 + index),
			selected ? Craft::Color::Cyan : Craft::Color::BrightWhite, 3000);
	}

	const std::string footer = canResume
		? "UP/DOWN + ENTER   ESC : RESUME"
		: "UP/DOWN + ENTER   ESC : BACK";
	Craft::Renderer::Get().Submit(footer,
		Craft::Vector2(screenWidth / 2 - 16, centerY + 4),
		Craft::Color::BrightWhite, 3000);
}
