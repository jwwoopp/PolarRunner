#pragma once

#include <Level/Level.h>

class TitleLevel : public Craft::Level
{
public:
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;

private:
	// 타이틀 화면은 로고, 게임 장면, 메뉴 세 덩어리로 나눠 그린다.
	void DrawLogo(int centerX);
	void DrawScene(int centerX);
	void DrawMenu(int centerX);

	// 별 반짝임, 배 흔들림, 물결 이동에 함께 쓰는 경과 시간.
	float animationTime = 0.0f;
};
