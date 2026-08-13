#pragma once

#include <Level/Level.h>
#include <functional>
#include <string>
#include <vector>

// PolarLevel 위에 겹쳐서(오버레이) 뜨는 일시정지 메뉴.
// PolarLevel은 Engine::SetSecondaryLevel/SetSecondaryLevelActive로 이 레벨을
// 켜고 끄기만 하고, 실행 중이던 게임 상태(state, traveledDistance 등)는 그대로
// 유지된다.
class MenuLevel : public Craft::Level
{
public:
	enum class Item
	{
		Resume,
		Retry,
		Quit,
		Length
	};

	MenuLevel(bool canResume, Item defaultItem);

	// 메뉴를 다시 열 때(예: 다른 상태에서 ESC) 새로 만들지 않고 문맥만 갱신.
	void SetContext(bool canResume, Item defaultItem);

	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;

private:
	struct MenuEntry
	{
		std::string text;
		std::function<void()> onSelected;
	};

	bool canResume = true;
	int currentIndex = 0;
	std::vector<MenuEntry> items;
};
